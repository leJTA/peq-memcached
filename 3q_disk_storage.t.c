#include "3q_disk_storage.h"
#include "third_party/minunit/minunit.h"

#include <sys/stat.h>

#ifdef UNIT_TESTING
char* base_dir();
char* filename();
#endif // UNIT_TESTING

MU_TEST(test_storage_init) {
   struct stat info;

   disk_storage_init("base-dir");

   mu_assert(base_dir() != NULL, "base-dir pointer should not be NULL");
   mu_assert(stat(base_dir(), &info) != 0, "base-dir folder must exists and be accessible");
}

MU_TEST_SUITE(test_suite) {
   MU_RUN_TEST(test_storage_init);
}

int main() {
   MU_RUN_SUITE(test_suite);
   MU_REPORT();
   return 0;
}