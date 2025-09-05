#ifndef __3Q_HISTORY_BUFFER_H__
#define __3Q_HISTORY_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Creators
bool history_buffer_init(size_t capacity);
void history_buffer_cleanup(void);

// Manipulators
void history_buffer_enqueue(char* key, uint8_t nkey);
void history_buffer_dequeue(void);
void history_buffer_remove(char* key, uint8_t nkey);
void history_buffer_lock(void);
void history_buffer_unlock(void);

// Accessors
bool history_buffer_is_empty(void);
bool history_buffer_contains(char* key, uint8_t nkey);
size_t history_buffer_size(void);


#endif // __3Q_HISTORY_BUFFER_H__