#ifndef __3Q_COMPRESSOR_H__
#define __3Q_COMPRESSOR_H__

#include <stdbool.h>

void compression_resources_init(void);
void compression_resources_cleanup(void);

// These methods are executed assuming that the item is already protected by a lock.
bool do_compress_item(item** it, LIBEVENT_THREAD *t);
bool do_decompress_item(item** it, LIBEVENT_THREAD *t);

#endif // __3Q_COMPRESSOR_H__