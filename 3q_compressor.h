#ifndef __3Q_COMPRESSOR_H__
#define __3Q_COMPRESSOR_H__

#include <stdbool.h>

void compression_resources_init(int nres);
void compression_resources_cleanup(void);

// These methods are executed assuming that the item is already protected by a lock.
bool do_compress_item(item** ptr);
bool do_decompress_item(item** ptr);

#endif // __3Q_COMPRESSOR_H__