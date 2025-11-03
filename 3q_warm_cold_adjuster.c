#include "3q_warm_cold_adjuster.h"
#include "memcached.h"

#include <stdint.h>
#include <string.h>


#define DISK_LATENCY_NS 150000  // 150'000 ns
#define DECOMP_LATENCY_NS 1000      // 1000 ns
#define RAM_LATENCY_NS 70           // 70ns
#define MIN_WARM_LRU_PCT 10
#define MAX_WARM_LRU_PCT 75
#define STEP_PCT 5                  // Adjustments are made by increments/decrements of 5%.
#define THRESHOLD 0.05              // 5%
#define WARM_COLD_ADJUSTER_SLEEP_MS 1000  // 1000 ms

static volatile int do_run_warm_cold_adjuster = 0;
static pthread_mutex_t warm_cold_adjuster_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t warm_cold_adjuster_tid;

static unsigned int lru_type_map[4] = {HOT_LRU, WARM_LRU, COLD_LRU, TEMP_LRU};

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

itemstats_t _stats_prev;
itemstats_t _stats_curr;
uint64_t _penalized_hits;
uint64_t _avoided_misses;
uint64_t _penalized_items;
double _G_previous = 0;
double _G_current = 0;

static void retrieve_stats()
{
   _stats_prev = _stats_curr;
   itemstats_t* itemstats = get_itemstats();
   int n;
   for (n = 0; n < MAX_NUMBER_OF_SLAB_CLASSES; n++) {
      int x;
      int i;
      for (x = 0; x < 4; x++) {
         i = n | lru_type_map[x];
         pthread_mutex_lock(&lru_locks[i]);
         _stats_curr.evicted += itemstats[i].evicted;
         _stats_curr.reclaimed += itemstats[i].reclaimed;
         _stats_curr.evicted_uncompressed += itemstats[i].evicted_uncompressed;
         _stats_curr.crawler_reclaimed += itemstats[i].crawler_reclaimed;
         _stats_curr.crawler_items_checked += itemstats[i].crawler_items_checked;
         _stats_curr.lrutail_reflocked += itemstats[i].lrutail_reflocked;
         _stats_curr.moves_to_cold += itemstats[i].moves_to_cold;
         _stats_curr.moves_to_warm += itemstats[i].moves_to_warm;
         _stats_curr.moves_within_lru += itemstats[i].moves_within_lru;
         _stats_curr.direct_reclaims += itemstats[i].direct_reclaims;
         pthread_mutex_unlock(&lru_locks[i]);
      }
   }
}

static double G()
{
   // alpha = compressed to cold / total evicted from warm
   double acceptance_rate = (double)(_stats_curr.moves_to_cold - _stats_prev.moves_to_cold) / 
                            ((_stats_curr.evicted_uncompressed - _stats_prev.evicted_uncompressed) + 
                             (_stats_curr.moves_to_cold - _stats_prev.moves_to_cold));
   _penalized_hits = _stats_curr.hits_penalized - _stats_prev.hits_penalized;
	_avoided_misses = (_stats_curr.hits_to_cold - _stats_prev.hits_to_cold) -
							(_stats_curr.hits_penalized - _stats_prev.hits_penalized);

	return acceptance_rate * _avoided_misses * (DISK_LATENCY_NS - DECOMP_LATENCY_NS) -
			 _penalized_hits * (DECOMP_LATENCY_NS - RAM_LATENCY_NS);
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
      
      retrieve_stats();
      _G_previous = _G_current;
      _G_current = G();
      double delta = (_G_current - _G_previous) / _G_previous;

      if (delta > THRESHOLD) {
         increase_cold_buffer_size();
      }
      else if (delta < -THRESHOLD) {
         decrease_cold_buffer_size();
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
      fprintf(stderr, "Failed to stop LRU maintainer thread: %s\n", strerror(ret));
      return -1;
   }
   return 0;
}