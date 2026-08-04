#include "peq_disk_storage.h"
#include "third_party/minunit/minunit.h"

#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef UNIT_TESTING
char* base_dir();
#endif // UNIT_TESTING

static void setup()
{
   disk_storage_init("/tmp/peq-test-dir");
}

static void teardown()
{
   disk_storage_cleanup();
}

MU_TEST(test_storage_init) {
   setup();
   struct stat info;

   mu_check(base_dir() != NULL);
   mu_assert(stat(base_dir(), &info) == 0, "Base directory must exists and be accessible");

   teardown();
}

MU_TEST(test_storage_init_default) {
   disk_storage_init(NULL);
   struct stat info;

   mu_check(base_dir() != NULL);
   mu_assert(stat(base_dir(), &info) == 0, "Default base directory folder must exists and be accessible");
   mu_assert(strcmp(base_dir(), "/tmp/peq-items-data") == 0, "Default base directory mismatch");

   teardown();
}

MU_TEST(test_write_and_read) {
   setup();
   const char* key = "key0";
   uint8_t nkey = 4;
   const char* in = "hello world";
   int nbytes = 11;
   char* out = (char*)malloc(nbytes * sizeof(char));

   size_t count1 = disk_storage_write(in, nbytes, key, nkey);
   size_t count2 = disk_storage_read(out, nbytes, key, nkey);

   mu_check(count1 == 11);
   mu_check(count2 == 11);
   mu_assert(memcmp(in, out, nbytes) == 0, "Input and output data are not equal");

   free(out);
   teardown();
}

MU_TEST(test_overwrite) {
   setup();
   const char* key = "key0";
   uint8_t nkey = 4;
   const char* in = "hello world";
   const char* new_in = "lorem ipsum";
   int nbytes = 11;
   char* out = (char*)malloc(nbytes * sizeof(char));

   size_t count1 = disk_storage_write(in, nbytes, key, nkey);
   size_t count2 = disk_storage_write(new_in, nbytes, key, nkey);
   size_t count3 = disk_storage_read(out, nbytes, key, nkey);

   mu_check(count1 == 11);
   mu_check(count2 == 11);
   mu_check(count3 == 11);
   mu_assert(memcmp(new_in, out, nbytes) == 0, "New input and output data are not equal");

   free(out);
   teardown();
}

MU_TEST(test_delete) {
   setup();
   const char* key = "key0";
   uint8_t nkey = 4;
   const char* in = "hello world";
   int nbytes = 11;
   char* out = (char*)malloc(nbytes * sizeof(char));

   size_t count1 = disk_storage_write(in, nbytes, key, nkey);
   bool success = disk_storage_delete(key, nkey);
   size_t count2 = disk_storage_read(out, nbytes, key, nkey);

   mu_check(count1 == 11);
   mu_check(success);
   mu_assert(count2 == 0, "Data should not remains in the filesystem");

   free(out);
   teardown();
}

MU_TEST(test_nonexistent_read) {
   setup();
   const char* key = "key0";
   uint8_t nkey = 4;
   int nbytes = 11;
   char* out = (char*)malloc(nbytes * sizeof(char));

   size_t count = disk_storage_read(out, nbytes, key, nkey);

   mu_assert(count == 0, "Data should not exists in the filesystem");

   free(out);
   teardown();
}

MU_TEST(test_read_smaller_data_size) {
   setup();
   const char* key = "key0";
   uint8_t nkey = 4;
   const char* in = "hello world";
   int nbytes = 11;
   char* out = (char*)malloc(nbytes * sizeof(char));

   size_t count1 = disk_storage_write(in, nbytes, key, nkey);
   size_t count2 = disk_storage_read(out, nbytes + 11, key, nkey);

   mu_check(count1 == 11);
   mu_assert(count2 == 11, "Read data size smaller than nbytes should succeed");

   free(out);
   teardown();
}

MU_TEST(test_unsafe_key) {
   setup();
   const char* key = ":k/e\\y~0";
   uint8_t nkey = 9;
   const char* in = "hello world";
   int nbytes = 11;
   char* out = (char*)malloc(nbytes * sizeof(char));

   size_t count1 = disk_storage_write(in, nbytes, key, nkey);
   size_t count2 = disk_storage_read(out, nbytes, key, nkey);

   mu_check(count1 == 11);
   mu_check(count2 == 11);
   mu_assert(memcmp(in, out, nbytes) == 0, "Input and output data are not equal");

   free(out);
   teardown();
}


MU_TEST_SUITE(test_suite) {
   MU_RUN_TEST(test_storage_init_default);
   MU_RUN_TEST(test_storage_init);
   MU_RUN_TEST(test_write_and_read);
   MU_RUN_TEST(test_overwrite);
   MU_RUN_TEST(test_delete);
   MU_RUN_TEST(test_nonexistent_read);
   MU_RUN_TEST(test_read_smaller_data_size);
   MU_RUN_TEST(test_unsafe_key);
}

int main() {
   MU_RUN_SUITE(test_suite);
   MU_REPORT();
   return MU_EXIT_CODE;
}