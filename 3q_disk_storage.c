#include "3q_disk_storage.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>

static char* _base_dir;
static int _maxlen;

void disk_storage_init(const char* base_dir)
{
   if (base_dir != NULL) {
      _base_dir = strdup(base_dir);
   }
   else {
      _base_dir = strdup("/tmp/3q-items-data");
   }
   if (mkdir(_base_dir, 0755) != 0) {
      if (errno != EEXIST) {
         fprintf(stderr, "[ERROR] Unable to create folder for items data\n");
         disk_storage_cleanup();
         exit(EXIT_FAILURE);
      }
   }
   // +1 for directory separator "/"
   // +UINT8_MAX for the maxlen of the key
   // +1 for the end of the char '\0'
   _maxlen = strlen(_base_dir) + 1 + UINT8_MAX + 1;
}

void disk_storage_cleanup(void)
{
   char cmd[512];
   snprintf(cmd, sizeof(cmd), "rm -rf %s", _base_dir);
   if (system(cmd) != 0) {
      fprintf( stderr, "[ERROR] unable to launch command : %s\n", cmd);
   }
   free(_base_dir);
}

static void safe_key_copy(char* safe_key, const char* key, uint8_t nkey)
{
   strncpy(safe_key, key, nkey);
   safe_key[nkey] = '\0';

   for (char *p = safe_key; *p != '\0'; p++) {
      switch (*p) {
      case '/':
      case '\\':
      case ':':
      case '~':
      case '*':
      case '?':
      case '"':
      case '\'':
      case '<':
      case '>':
      case '|':
      case ' ':
      case '\t':
      case '\n':
      case '\r':
         *p = '_';
         break;
      default:
            // valid char, does nothing
         break;
      }
   }
}

size_t disk_storage_read(void* ptr, size_t ntotal, const char* key, uint8_t nkey)
{
   char* filename = (char*)malloc(_maxlen * sizeof(char));
   char safe_key[UINT8_MAX + 1];

   safe_key_copy(safe_key, key, nkey);
   snprintf(filename, _maxlen, "%s/%s", _base_dir, safe_key);
   int fd = open(filename, O_RDONLY);
   
   if (fd < 0) {
      perror("open");
      return 0;
   }
   
   size_t bytes_read = 0;
   while (bytes_read < ntotal) {
      ssize_t r = read(fd, (char*)ptr + bytes_read, ntotal - bytes_read);
      if (r < 0) {
         if (errno == EINTR) continue; // retry
         perror("read");
         flock(fd, LOCK_UN);
         close(fd);
         return 0;
      }
      if (r == 0) break; // EOF
      bytes_read += r;
   }

   close(fd);
   return bytes_read;
}

size_t disk_storage_write(const void* ptr, size_t ntotal, const char* key, uint8_t nkey)
{
   char* filename = (char*)malloc(_maxlen * sizeof(char));
   char safe_key[UINT8_MAX + 1];

   safe_key_copy(safe_key, key, nkey);
   snprintf(filename, _maxlen, "%s/%s", _base_dir, safe_key);

   int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644);
   if (fd < 0) {
      perror("open");
      return -1;
   }

   size_t bytes_written = 0;
   while (bytes_written < ntotal) {
      ssize_t w = write(fd, (char*)ptr + bytes_written, ntotal - bytes_written);
      if (w < 0) {
         if (errno == EINTR) continue; // retry
         perror("write");
         flock(fd, LOCK_UN);
         close(fd);
         return -1;
      }
      bytes_written += w;
   }

   fsync(fd); // force write to the disk
   close(fd);
   return bytes_written;
}

bool disk_storage_delete(const char* key, uint8_t nkey)
{
   char* filename = (char*)malloc(_maxlen * sizeof(char));
   char safe_key[UINT8_MAX + 1];

   safe_key_copy(safe_key, key, nkey);
   snprintf(filename, _maxlen, "%s/%s", _base_dir, safe_key);
   return (remove(filename) == 0);
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