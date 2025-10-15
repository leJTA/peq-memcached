#ifndef __3Q_HISTORY_BUFFER_H__
#define __3Q_HISTORY_BUFFER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned int rel_time_t;
typedef struct history_item {
	char* key;
	rel_time_t exptime;
	int nbytes;
	uint16_t it_flags;
	uint8_t slabs_clsid;
	uint8_t nkey;
	struct history_item* next;
} history_item;

// Creators
bool history_buffer_init(size_t capacity);
void history_buffer_cleanup(void);
void destroy_history_item(history_item* hi);

// Manipulators
void history_buffer_enqueue(const char* key, uint8_t nkey, rel_time_t exptime, int nbytes,
									 uint16_t it_flags, uint8_t slabs_clsid);
void history_buffer_dequeue(void);
history_item* history_buffer_remove(const char* key, uint8_t nkey);
void history_buffer_lock(void);
void history_buffer_unlock(void);

// Accessors
void history_buffer_print(void);
bool history_buffer_is_empty(void);
bool history_buffer_contains(const char* key, uint8_t nkey);
size_t history_buffer_size(void);
size_t history_buffer_capacity(void);
size_t history_buffer_max_mem_usage(void);

#endif // __3Q_HISTORY_BUFFER_H__