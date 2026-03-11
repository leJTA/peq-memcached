#ifndef __3Q_COMPRESSOR_H__
#define __3Q_COMPRESSOR_H__

#include <stdbool.h>

void compression_resources_init(void);
void compression_resources_cleanup(void);

double get_decompression_bw(void);
double get_read_memory_bw(void);
size_t get_average_size(void);

// These methods are executed assuming that the item is already protected by a lock.
bool do_compress_item(item** ptr);
bool do_decompress_item(item** ptr);

#endif // __3Q_COMPRESSOR_H__