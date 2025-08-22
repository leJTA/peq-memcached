#include "3q_compressor.h"

#include <stdlib.h>
#include <zstd.h>
#include <lz4.h>
#include <zlib.h>

// Compression context
struct compression_resources {
   char* buffer;
   size_t buffer_size;
   ZSTD_CCtx* cctx;
   ZSTD_DCtx* dctx;
};

static struct compression_resources* _rcs;

void compression_resources_init()
{
   _rcs = calloc(settings.num_threads, sizeof(struct compression_resources));
   if (_rcs == NULL) {
      fprintf(stderr, "ERROR: unable to allocate memory for compression ressources\n");
      exit(EXIT_FAILURE);
   }

   for (int i = 0; i < settings.num_threads; ++i) {
      _rcs[i].buffer_size = ZSTD_compressBound(settings.item_size_max);
      _rcs[i].buffer = calloc(_rcs[i].buffer_size, sizeof(char));
      _rcs[i].cctx = ZSTD_createCCtx();
      _rcs[i].dctx = ZSTD_createDCtx();
   }
}

void compression_resources_cleanup()
{
   for (int i = 0; i < settings.num_threads; ++i) {
      free(_rcs[i].buffer);
      ZSTD_freeCCtx(_rcs[i].cctx);
   }
}


bool do_compress_item(item* it, enum compression_algorithm ca, LIBEVENT_THREAD *t)
{
   if (it == NULL) {
      fprintf(stderr, "ERROR: unable to compress item, pointer is NULL\n");
      exit(EXIT_FAILURE);
   }
   assert(ITEM_lruid(it) | COLD_LRU);

   struct compression_resources rc = _rcs[t->thread_baseid];
   switch (ca) {
      case ZSTD:
         size_t compressed_size = ZSTD_compressCCtx(rc.cctx, rc.buffer, rc.buffer_size, ITEM_data(it), it->nbytes, 1);
         
         // If the compression ratio is not high enough, the compression is dropped
         if (((double) it->nbytes / compressed_size) < settings.compression_ratio_min) {
            return false;
         }
         
         // Shrinking the memory allocated to the item
         int new_ntotal = ITEM_ntotal(it) - (it->nbytes - compressed_size);
         item* tmp = realloc(it, new_ntotal * sizeof(char));
         if (tmp == NULL) {
            fprintf(stderr, "ERROR: Unable to shrink the memory allocated to the item\n");
            // free(it);
            exit(EXIT_FAILURE);
         }
         it->nbytes = compressed_size;
         it = tmp;
         break;
         
      case LZ4:
         break;
      case ZLIB:
         break;
   }
   
   return true;
}

void do_decompress_item(item* it, enum compression_algorithm ca, LIBEVENT_THREAD *t)
{
   if (it == NULL) {
      fprintf(stderr, "ERROR: unable to decompress item, pointer is NULL\n");
      exit(EXIT_FAILURE);
   }
   assert(ITEM_lruid(it) | COLD_LRU);

   struct compression_resources rc = _rcs[t->thread_baseid];
   switch (ca) {
      case ZSTD:
         size_t decompressed_size = ZSTD_decompressDCtx(rc.dctx, rc.buffer, rc.buffer_size, ITEM_data(it), it->nbytes);
         assert(it->nbytes < decompressed_size);

         // Expanding the memory allocated to the item
         int new_ntotal = ITEM_ntotal(it) + (decompressed_size - it->nbytes);
         item* tmp = realloc(it, decompressed_size * sizeof(char));
         if (tmp == NULL) {
            fprintf(stderr, "ERROR: unable to expand item allocated size\n");
            // free(it);
            exit(EXIT_FAILURE);
         }
         it->nbytes = decompressed_size;
         it = tmp;
         break;
         
      case LZ4:
         break;
      case ZLIB:
         break;
   }
}