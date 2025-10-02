#include "3q_buffer_pool.h"

#include <stdlib.h>

typedef struct buffer_pool {
   char* data;
   size_t buffer_size;
   int buffer_count;
} buffer_pool;

static buffer_pool _pool;

bool buffer_pool_init(int buffer_count, size_t buffer_size)
{
   _pool.buffer_count = buffer_count;
   _pool.buffer_size = buffer_size;
   _pool.data = malloc(buffer_count * buffer_size * sizeof(char));
   if (_pool.data == NULL) {
		return false;
   }
   return true;
}

void buffer_pool_cleanup(void)
{
   if (_pool.data != NULL) {
      free(_pool.data);
      _pool.data = NULL;
      _pool.buffer_count = 0;
      _pool.buffer_size = 0;
   }
}

int buffer_pool_count(void)
{
   return _pool.buffer_count;
}

size_t buffer_pool_bufsize(void)
{
   return _pool.buffer_size;
}

void* buffer_pool_data(int buffer_id)
{
   if (buffer_id < 0 || buffer_id >= _pool.buffer_count) return NULL;
   return _pool.data + (buffer_id * _pool.buffer_size);
}