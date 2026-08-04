#include "peq_warm_cold_adjuster.h"
#include "memcached.h"
#include "peq_compressor.h"

#include <stdint.h>
#include <string.h>
#include <math.h>

#define MIN_WARM_LRU_PCT 10
#define MAX_WARM_LRU_PCT 79
#define STEP_PCT 0                  // Adjustments are made by increments/decrements of 1%.
#define THRESHOLD 0.05              // 5%
#define WARM_COLD_ADJUSTER_SLEEP_MS 5000  // 5000 ms

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

static const double disk_bw = 429916.16;    // B/ms <=> 410 MB/s (SATA SSD 6 Gbps random read 256KB)

static itemstats_t _stats_prev;
static itemstats_t _stats_curr;
static uint64_t _penalized_hits;
static uint64_t _avoided_misses;
static double _acceptance_rate = 0;
static double _G_prev = 0;
static double _G_curr = 0;
static ssize_t _used_bytes;
static double _used_pct;

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
   double ram_bw = get_read_memory_bw();
   double hit_lat = get_hit_latency();
   double pen_hit_lat = get_pen_hit_latency();
   size_t d = get_average_size();
   if (settings.verbose > 0) {
      fprintf(stderr, "[DEBUG] Penalized hits = %ld\n"
                      "        Avoided misses = %ld\n" 
                      "        decomp bw      = %.3f (GB/s)\n"
                      "        memory bw      = %.3f (GB/s)\n"
                      "        latency :\n"
                      "            hit latency      = %.3f (us)\n"
                      "            pen. hit latency = %.3f (us)\n"
                      "            factor           = %.3fx\n",
                      _penalized_hits, _avoided_misses, (decomp_bw / 1073741.824), 
                      (ram_bw / 1073741.824), hit_lat, pen_hit_lat, pen_hit_lat / hit_lat);
   }
	
   return _avoided_misses * (d / disk_bw - d / decomp_bw) -
          _penalized_hits * (d / decomp_bw - d / ram_bw) / _acceptance_rate;
}

static void increase_cold_buffer_size()
{
   _used_bytes = cold_lru_bytes();
   _used_pct = _used_bytes * 100.0 / settings.maxbytes;
   int allocated_pct = 100 - settings.hot_lru_pct - settings.warm_lru_pct;
   if (fabs(_used_pct - allocated_pct) > 0.5) return;
   
   if (settings.warm_lru_pct > MIN_WARM_LRU_PCT) {
      settings.warm_lru_pct -= STEP_PCT;
   }
}

static void decrease_cold_buffer_size()
{
   _used_bytes = cold_lru_bytes();
   _used_pct = _used_bytes * 100.0 / settings.maxbytes;
   int allocated_pct = 100 - settings.hot_lru_pct - settings.warm_lru_pct;
   if (fabs(_used_pct - allocated_pct) > 0.5) return;

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

      if (settings.verbose > 0) {
         // fprintf(stderr, "%.1f,%.1f,%.2f,%d\n", _G_curr, _G_prev, delta, settings.warm_lru_pct);
         fprintf(stderr, "[DEBUG] G_curr   = %.3f\n"
                         "        G_prev   = %.3f\n"
                         "        delta    = %.3f\n"
                         "        cold_lru\n"
                         "            allocated_pct = %d%%\n"
                         "            used_pct      = %.2f%% (%.2f MB)\n",
                         _G_curr, _G_prev, delta, 100 - settings.hot_lru_pct - settings.warm_lru_pct,
                         _used_pct, _used_bytes / (1024 * 1024.0));
      }
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
   thread_setname(warm_cold_adjuster_tid, "peq-adjuster");
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