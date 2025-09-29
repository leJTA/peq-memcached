#include "3q_disk_storage.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

static char* _base_dir;
static char* _filename;
static int _maxlen;
static char _safe_key[UINT8_MAX + 1];

void disk_storage_init(const char* base_dir)
{
   if (base_dir != NULL) {
      _base_dir = strdup(base_dir);
   }
   else {
      _base_dir = "items-data";
   }
   // +1 for directory separator "/"
   // +UINT8_MAX for the maxlen of the key
   // +1 for the end of the char '\0'
   _maxlen = strlen(_base_dir) + 1 + UINT8_MAX + 1;
   _filename = (char*)calloc(_maxlen, sizeof(char));
}

static void safe_key_copy(const char* key, uint8_t nkey)
{
   strncpy(_safe_key, key, nkey);
   _safe_key[nkey] = '\0';

   for (char *p = _safe_key; *p != '\0'; p++) {
      if (*p == '/' || *p == '\\' || *p == ':' || *p == '~') {
         *p = '_';
      }
   }
}

bool disk_storage_read(void* ptr, int nbytes, const char* key, uint8_t nkey)
{
   safe_key_copy(key, nkey);
   snprintf(_filename, _maxlen, "%s/%s", _base_dir, _safe_key);
   strncat(_filename, key, nkey);
   FILE* file = fopen(_filename, "r");

   if (!file) return false;
   size_t count = fread((char*)ptr, nbytes, sizeof(char), file);
   fclose(file);

   return (count == (size_t)nbytes);
}

bool disk_storage_write(const void* ptr, int nbytes, const char* key, uint8_t nkey)
{
   safe_key_copy(key, nkey);
   snprintf(_filename, _maxlen, "%s/%s", _base_dir, _safe_key);
   FILE* file = fopen(_filename, "w");

   if (!file) return false;
   size_t count = fwrite((char*)ptr, nbytes, sizeof(char), file);
   fclose(file);

   return (count == (size_t)nbytes);
}

bool disk_storage_delete(const char* key, uint8_t nkey)
{
   char* filename = strndup(key, nkey);
   return (remove(filename) != 0);
}

#ifdef UNIT_TESTING
char* base_dir()
{
   return _base_dir;
}

char* filename()
{
   return _filename;
}
#endif // UNIT_TESTING