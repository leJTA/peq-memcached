#include "3q_warm_cold_adjuster.h"
#include "memcached.h"

#include <stdint.h>

#define MIN_COLD_LRU_PCT = 10;
#define PCT_ADJUSTMENT_INCREMENT = 1; // Adjustments are made by increments/decrements of 1%.


typedef struct _3q_stats {
   double acceptance_rate;
   uint64_t penalized_hits;
   uint64_t avoided_misses;
   uint64_t penalized_items;
   int avg_item_size;
} _3q_stats;

// void warm_cold_adjust()
// {

// }

// int get_item_position(item* it, int lruid)
// {
   
// }

// int get_penalized_item_count()
// {
   
// }