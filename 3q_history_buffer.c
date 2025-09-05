#include "3q_history_buffer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void destroy_history_item(history_item* hi)
{
   free(hi->key);
   free(hi);
}

history_buffer* create_history_buffer(size_t capacity)
{
   if(capacity < 1) return NULL;

   history_buffer* history = malloc(sizeof(history_buffer));
   history->head = NULL;
   history->tail = NULL;
   history->capacity = capacity;
   history->size = 0;

   return history;
}

void destroy_history_buffer(history_buffer* history)
{
   if (history == NULL) return;

   history_item* hi = history->head;
   history_item* nxt = NULL;
   while (hi != NULL)
   {
      nxt = hi->next;
      destroy_history_item(hi);
      hi = nxt;
   }
   free(history);
}

void history_buffer_enqueue(history_buffer* history, char* key, uint8_t nkey)
{
   if (history->size == history->capacity) {
      history_buffer_dequeue(history);
   }

   history_item* hi = malloc(sizeof(history_item));
   hi->key = malloc(nkey * sizeof(char));
   memcpy(hi->key, key, nkey);
   hi->nkey = nkey;

   if (history->head == NULL) {
      history->head = hi;
      history->tail = hi;
   }
   else {
      history->tail->next = hi;
      history->tail = hi;
   }
   hi->next = NULL;

   history->size++;
}

void history_buffer_dequeue(history_buffer* history)
{
   if (history->head == NULL) {
      return;
   }

   history_item* hi = history->head;
   history->head = history->head->next;
   if (history->head == NULL) {
      history->tail = NULL;
   }
   destroy_history_item(hi);

   history->size--;
}

void history_buffer_remove(history_buffer* history, char* key, uint8_t nkey)
{
   history_item* hi = history->head;
   history_item* prev = NULL;
   
   while (hi != NULL)
   {
      if (memcmp(hi->key, key, nkey) == 0) {
         if (prev != NULL) {
            prev->next = hi->next;
         }
         
         if (hi->next == NULL) {
            history->tail = prev;
         }

         destroy_history_item(hi);
         break;
      }
      prev = hi;
      hi = hi->next;
   }

   history->size--;
}

bool history_buffer_is_empty(history_buffer* history)
{
   return (history->size == 0);
}

bool history_buffer_contains(history_buffer* history, char* key, uint8_t nkey)
{
   history_item* hi = history->head;
   while (hi != NULL)
   {
      if (hi->nkey == nkey && memcmp(hi->key, key, nkey) == 0) {
         return true;
      }
      hi = hi->next;
   }
   return false;
}