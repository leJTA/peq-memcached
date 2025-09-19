#include "3q_history_buffer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>

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

static history_buffer* _history = NULL;
static pthread_mutex_t _history_lock = PTHREAD_MUTEX_INITIALIZER;

static void destroy_history_item(history_item* hi)
{
   free(hi->key);
   free(hi);
}

bool history_buffer_init(size_t capacity)
{
   if(capacity < 1) {
      return false;
   }

   _history = malloc(sizeof(history_buffer));
   _history->head = NULL;
   _history->tail = NULL;
   _history->capacity = capacity;
   _history->size = 0;

   return true;
}

void history_buffer_cleanup(void)
{
   if (_history == NULL) return;

   history_item* hi = _history->head;
   history_item* nxt = NULL;
   while (hi != NULL)
   {
      nxt = hi->next;
      destroy_history_item(hi);
      hi = nxt;
   }
   free(_history);
   _history = NULL;
}

void history_buffer_enqueue(const char* key, uint8_t nkey)
{
   if (_history->size == _history->capacity) {
      history_buffer_dequeue();
   }

   history_item* hi = malloc(sizeof(history_item));
   hi->key = malloc(nkey * sizeof(char));
   memcpy(hi->key, key, nkey);
   hi->nkey = nkey;

   if (_history->head == NULL) {
      _history->head = hi;
      _history->tail = hi;
   }
   else {
      _history->tail->next = hi;
      _history->tail = hi;
   }
   hi->next = NULL;

   _history->size++;
}

void history_buffer_dequeue(void)
{
   if (_history->head == NULL) {
      return;
   }

   history_item* hi = _history->head;
   _history->head = _history->head->next;
   if (_history->head == NULL) {
      _history->tail = NULL;
   }
   destroy_history_item(hi);

   _history->size--;
}

void history_buffer_remove(const char* key, uint8_t nkey)
{
   history_item* hi = _history->head;
   history_item* prev = NULL;
   
   while (hi != NULL)
   {
      if (hi->nkey == nkey && memcmp(hi->key, key, nkey) == 0) {
         if (prev == NULL) {
            _history->head = hi->next;
         }
         else {
            prev->next = hi->next;
         }
         
         if (hi->next == NULL) {
            _history->tail = prev;
         }

         destroy_history_item(hi);
         break;
      }
      prev = hi;
      hi = hi->next;
   }

   _history->size--;
}

void history_buffer_lock(void)
{
   pthread_mutex_lock(&_history_lock);
}

void history_buffer_unlock(void)
{
   pthread_mutex_unlock(&_history_lock);
}


bool history_buffer_is_empty(void)
{
   return (_history->size == 0);
}

bool history_buffer_contains(const char* key, uint8_t nkey)
{
   history_item* hi = _history->head;
   while (hi != NULL)
   {
      if (hi->nkey == nkey && memcmp(hi->key, key, nkey) == 0) {
         return true;
      }
      hi = hi->next;
   }
   return false;
}

size_t history_buffer_size(void)
{
   return _history->size;
}

size_t history_buffer_capacity(void)
{
   return _history->capacity;
}

size_t history_buffer_max_mem_usage(void)
{
   if (!_history) {
      return 0;
   }
   return sizeof(history_buffer) + _history->capacity * (sizeof(history_item) + UINT8_MAX * sizeof(char));
}

#ifdef UNIT_TESTING
void* history_buffer_tail()
{
   return _history->tail;
}

void* history_buffer_head()
{
   return _history->head;
}
#endif // UNIT_TESTING