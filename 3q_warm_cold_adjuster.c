#include "3q_warm_cold_adjuster.h"
#include "memcached.h"
#include "3q_compressor.h"

#include <stdint.h>
#include <string.h>
#include <math.h>

// #define DECOMP_BW_ZSTD 1342177  // 1342177 B/ms <=> 1.2 GB/s (Zstd avg decomp Bandwidth)
// #define DECOMP_BW_LZ4 3758096 // 3758096 B/ms <=> 3.5 GB/s (LZ4 avg decomp Bandwidth)

#define MIN_WARM_LRU_PCT 10
#define MAX_WARM_LRU_PCT 75
#define STEP_PCT 5                  // Adjustments are made by increments/decrements of 5%.
#define THRESHOLD 0.05              // 5%
#define WARM_COLD_ADJUSTER_SLEEP_MS 1000  // 1000 ms

static volatile int do_run_warm_cold_adjuster = 0;
static pthread_mutex_t warm_cold_adjuster_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t warm_cold_adjuster_tid;

typedef struct {
   uint64_t evicted;
   uint64_t evicted_nonzero;
   uint64_t reclaimed;
   uint64_t outofmemory;
   uint64_t tailrepairs;
   uint64_t evicted_uncompressed;
   uint64_t crawler_reclaimed;
   uint64_t crawler_items_checked;
   uint64_t lrutail_reflocked;
   uint64_t moves_to_history_buffer;
   uint64_t moves_to_cold;
   uint64_t moves_to_warm;
   uint64_t moves_within_lru;
   uint64_t direct_reclaims;
   uint64_t hits_to_hot;
   uint64_t hits_to_warm;
   uint64_t hits_to_cold;
   uint64_t hits_penalized;
   uint64_t hits_to_temp;
   uint64_t mem_requested;
   rel_time_t evicted_time;
} itemstats_t;

static const double disk_bw = 805306.368;    // 805306 B/ms <=> 0.75 GB/s (SATA SSD 6 Gbps)
static const double ram_bw = 91268055.312;   // 91268055 B/ms <=> 85 GB/s (DDR4 2666MHz dual channel)

static itemstats_t _stats_prev;
static itemstats_t _stats_curr;
static uint64_t _penalized_hits;
static uint64_t _avoided_misses;
static double _acceptance_rate = 0;
static double _G_prev = 0;
static double _G_curr = 0;

static void retrieve_stats(itemstats_t* stats)
{
   itemstats_t* itemstats = get_itemstats();
   struct thread_stats thread_stats;
   threadlocal_stats_aggregate(&thread_stats);
   memset(stats, 0, sizeof(itemstats_t));
   for (int n = 0; n < MAX_NUMBER_OF_SLAB_CLASSES; n++) {
      int i = n | COLD_LRU;
      int j = n | WARM_LRU;   // for moves_to_cold
      pthread_mutex_lock(&lru_locks[i]);
      stats->evicted += itemstats[i].evicted;
      stats->evicted_uncompressed += itemstats[i].evicted_uncompressed;
      stats->moves_to_cold += itemstats[j].moves_to_cold;      // !!
      stats->hits_penalized += thread_stats.lru_hits_penalized[i];
      stats->hits_to_cold += thread_stats.lru_hits[i];
      pthread_mutex_unlock(&lru_locks[i]);
   }
}

static double G()
{
   // tau = compressed to cold / total evicted from warm
   _acceptance_rate = (double)(_stats_curr.moves_to_cold - _stats_prev.moves_to_cold) /
                            ((_stats_curr.evicted_uncompressed - _stats_prev.evicted_uncompressed) + 
                             (_stats_curr.moves_to_cold - _stats_prev.moves_to_cold));
   _penalized_hits = _stats_curr.hits_penalized - _stats_prev.hits_penalized;
	_avoided_misses = (_stats_curr.hits_to_cold - _stats_prev.hits_to_cold) -
							(_stats_curr.hits_penalized - _stats_prev.hits_penalized);

   double decomp_bw = get_decompression_bw();
   size_t d = get_average_size(); 
   fprintf(stderr, "[DEBUG] penalized hits = %ld, avoided_misses = %ld, decomp_bw = %f (GB/s)\n", _penalized_hits, _avoided_misses, (decomp_bw / 1073741.824));
   // fprintf(stderr, "[DEBUG] size = %ld, tau = %f\n", d, _acceptance_rate);
	return _avoided_misses * (d / disk_bw - d / decomp_bw) -
			 _penalized_hits * (d / decomp_bw - d / ram_bw) / _acceptance_rate;
}

static void increase_cold_buffer_size()
{
   if (settings.warm_lru_pct > MIN_WARM_LRU_PCT) {
      settings.warm_lru_pct -= STEP_PCT;
   }
}

static void decrease_cold_buffer_size()
{
   if (settings.warm_lru_pct < MAX_WARM_LRU_PCT) {
      settings.warm_lru_pct += STEP_PCT;
   }
}

// Main loop
static void* warm_cold_adjuster_thread()
{
   int to_sleep = WARM_COLD_ADJUSTER_SLEEP_MS;

   pthread_mutex_lock(&warm_cold_adjuster_lock);
   while (do_run_warm_cold_adjuster) {
      pthread_mutex_unlock(&warm_cold_adjuster_lock);
      usleep(to_sleep * 1000);
      pthread_mutex_lock(&warm_cold_adjuster_lock);
      
      _stats_prev = _stats_curr;
      _G_prev = _G_curr;
      retrieve_stats(&_stats_curr);
      _G_curr = G();
      double delta = (_G_curr - _G_prev) / fabs(_G_prev);

      if (delta < -THRESHOLD || _G_curr < 0) {
         decrease_cold_buffer_size();
      }
      else if (delta > THRESHOLD) {
         increase_cold_buffer_size();
      }

      // fprintf(stderr, "%.1f,%.1f,%.2f,%d\n", _G_curr, _G_prev, delta, settings.warm_lru_pct);
      fprintf(stderr, "[DEBUG] G_curr = %.3f, G_prev = %.3f, delta = %.3f, cold_pct = %d\n", _G_curr, _G_prev, delta, 100 - settings.hot_lru_pct - settings.warm_lru_pct);
   }

   return NULL;
}

int start_warm_cold_adjuster_thread(void *arg) 
{
   int ret;

   pthread_mutex_lock(&warm_cold_adjuster_lock);
   do_run_warm_cold_adjuster = 1;
   if ((ret = pthread_create(&warm_cold_adjuster_tid, NULL,
      warm_cold_adjuster_thread, arg)) != 0) {
      fprintf(stderr, "Can't create WARM-COLD adjuster thread: %s\n",
         strerror(ret));
      pthread_mutex_unlock(&warm_cold_adjuster_lock);
      return -1;
   }
   thread_setname(warm_cold_adjuster_tid, "mc-warm-cold-adjuster");
   pthread_mutex_unlock(&warm_cold_adjuster_lock);

   return 0;
}

int stop_warm_cold_adjuster_thread(void)
{
   int ret;
   pthread_mutex_lock(&warm_cold_adjuster_lock);
   do_run_warm_cold_adjuster = 0;
   pthread_mutex_unlock(&warm_cold_adjuster_lock);
   if ((ret = pthread_join(warm_cold_adjuster_tid, NULL)) != 0) {
      fprintf(stderr, "Failed to stop WARM-COLD maintainer thread: %s\n", strerror(ret));
      return -1;
   }
   return 0;
}