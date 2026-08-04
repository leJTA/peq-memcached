#ifndef __PEQ_BUFFER_POOL_H__
#define __PEQ_BUFFER_POOL_H__

#include <stddef.h>
#include <stdbool.h>

// Creators
bool buffer_pool_init(int buffer_count, size_t buffer_size);
void buffer_pool_cleanup(void);

// Accessors
int buffer_pool_count(void);
size_t buffer_pool_bufsize(void);
void* buffer_pool_data(int id);

#endif // __PEQ_BUFFER_POOL_H__