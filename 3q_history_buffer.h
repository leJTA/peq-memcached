#ifndef __3Q_HISTORY_BUFFER_H__
#define __3Q_HISTORY_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// forward declaration
typedef struct history_item history_item;
typedef struct history_buffer history_buffer;

// type definition
struct history_item {
   char* key;
   uint8_t nkey;
   history_item* next;
};

struct history_buffer {
   history_item* head;
   history_item* tail;
   size_t capacity;
   size_t size;
};

// Creators
history_buffer* create_history_buffer(size_t capacity);
void destroy_history_buffer(history_buffer* history);

// Manipulators
void history_buffer_enqueue(history_buffer* history, char* key, uint8_t nkey);
void history_buffer_dequeue(history_buffer* history);
void history_buffer_remove(history_buffer* history, char* key, uint8_t nkey);

// Accessors
bool history_buffer_is_empty(history_buffer* history);
bool history_buffer_contains(history_buffer* history, char* key, uint8_t nkey);


#endif // __3Q_HISTORY_BUFFER_H__