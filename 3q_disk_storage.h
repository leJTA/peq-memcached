#ifndef __3Q_DISK_STORAGE_H__
#define __3Q_DISK_STORAGE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// CREATORS
void disk_storage_init(const char* base_dir);
void disk_storage_cleanup(void);

// MEANIPULATORS

// ACCESSORS
bool disk_storage_read(void* ptr, int nbytes,const char* key, uint8_t nkey);
bool disk_storage_write(const void* ptr, int nbytes,const char* key, uint8_t nkey);
bool disk_storage_delete(const char* key, uint8_t nkey);

#endif // __3Q_DISK_STORAGE_H__