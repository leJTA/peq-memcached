#ifndef __3Q_DISK_STORAGE_H__
#define __3Q_DISK_STORAGE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Creators
void disk_storage_init(const char* base_dir);
void disk_storage_cleanup(void);

// Accessors
size_t disk_storage_read(void* ptr, size_t ntotal,const char* key, uint8_t nkey);
size_t disk_storage_write(const void* ptr, size_t ntotal,const char* key, uint8_t nkey);
bool disk_storage_delete(const char* key, uint8_t nkey);

#endif // __3Q_DISK_STORAGE_H__