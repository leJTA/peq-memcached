#ifndef __3Q_COMPRESSOR_H__
#define __3Q_COMPRESSOR_H__

#include "memcached.h"
#include "items.h"

enum compression_algorithm {
   ZSTD,
   LZ4,
   ZLIB
};

// These methods are executed assuming that the item is already protected by a lock.

void compression_resources_init();
void compression_resources_cleanup();

bool do_compress_item(item* it, enum compression_algorithm ca, LIBEVENT_THREAD *t);
void do_decompress_item(item* it, enum compression_algorithm ca, LIBEVENT_THREAD *t);

#endif // __3Q_COMPRESSOR_H__