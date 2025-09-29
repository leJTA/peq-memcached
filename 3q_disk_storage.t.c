#include "3q_disk_storage.h"
#include "third_party/minunit/minunit.h"

#include <sys/stat.h>
#include <unistd.h>

#ifdef UNIT_TESTING
char* base_dir();
char* filename();
#endif // UNIT_TESTING

MU_TEST(test_storage_init) {
   struct stat info;

   disk_storage_init("/tmp/3q-test-dir");

   mu_check(base_dir() != NULL);
   mu_assert(stat(base_dir(), &info) != 0, "base-dir folder must exists and be accessible");

   system("rm -rf /tmp/3q-test-dir");
}

MU_TEST(test_write_and_read) {

}

MU_TEST(test_overwrite) {

}

MU_TEST(test_delete) {

}

MU_TEST(test_nonexistent_read) {

}


MU_TEST_SUITE(test_suite) {
   MU_RUN_TEST(test_write_and_read);
   MU_RUN_TEST(test_overwrite);
   MU_RUN_TEST(test_delete);
   MU_RUN_TEST(test_nonexistent_read);
}

int main() {
   MU_RUN_SUITE(test_suite);
   MU_REPORT();
   return MU_EXIT_CODE;
}