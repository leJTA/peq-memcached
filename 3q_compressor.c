#include "memcached.h"
#include "3q_compressor.h"

#include <stdlib.h>
#include <string.h>
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

void compression_resources_init(void)
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

void compression_resources_cleanup(void)
{
   for (int i = 0; i < settings.num_threads; ++i) {
      free(_rcs[i].buffer);
      ZSTD_freeCCtx(_rcs[i].cctx);
   }
}


bool do_compress_item(item* it, enum compression_algorithm ca, LIBEVENT_THREAD *t)
{
   if (it == NULL) {
      fprintf(stderr, "[ERROR] unable to compress item, pointer is NULL\n");
      exit(EXIT_FAILURE);
   }
   // assert(ITEM_lruid(it) | COLD_LRU);

   struct compression_resources rc = _rcs[t->thread_baseid];
   size_t compressed_size = 0;
   switch (ca) {
      case ZSTD:
         compressed_size = ZSTD_compressCCtx(rc.cctx, rc.buffer, rc.buffer_size, ITEM_data(it), it->nbytes, 1);
         if (ZSTD_isError(compressed_size)) {
            fprintf(stderr, "[ERROR] Zstd compression failed : %s \n", ZSTD_getErrorName(compressed_size));
            exit(EXIT_FAILURE);
         }
         // If the compression ratio is not high enough, the compression is dropped
         if (((double) it->nbytes / compressed_size) < settings.compression_ratio_min) {
            return false;
         }
         break;

      case LZ4:
         break;

      case ZLIB:
         break;

   }

   fprintf(stderr, "[DEBUG] old_bytes = %d, new_bytes = %lu\n", it->nbytes, compressed_size);
   memcpy(ITEM_data(it), rc.buffer, compressed_size *  sizeof(char));
   it->nbytes = compressed_size;
   // Move the item to a slab class with a smaller item size
   // TODO: call function to change slab class
   
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
   size_t decompressed_size = 0;
   switch (ca) {
      case ZSTD:
         decompressed_size = ZSTD_decompressDCtx(rc.dctx, rc.buffer, rc.buffer_size, ITEM_data(it), it->nbytes);
         if (ZSTD_isError(decompressed_size)) {
            fprintf(stderr, "[ERROR] Zstd decompression failed : %s \n", ZSTD_getErrorName(decompressed_size));
            exit(EXIT_FAILURE);
         }
         assert(it->nbytes < decompressed_size);
         break;

      case LZ4:
         break;
         
      case ZLIB:
         break;
   }

   memcpy(ITEM_data(it), rc.buffer, decompressed_size *  sizeof(char));
   it->nbytes = decompressed_size;
   // Move the item to a slab class with a larger item size
   // TODO: call function to change slab class
}