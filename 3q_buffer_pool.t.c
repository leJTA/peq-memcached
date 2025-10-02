#include "3q_buffer_pool.h"
#include "third_party/minunit/minunit.h"

#include <string.h>

int tests_run = 0;

MU_TEST(test_init_and_cleanup) {
    bool success = buffer_pool_init(4, 1048576);

    mu_assert(success, "buffer_pool_init should succeed");
    mu_assert(buffer_pool_count() == 4, "buffer_pool_count should be 4");
    mu_assert(buffer_pool_bufsize() == 1048576, "buffer_pool_bufsize should be 1MB");

    buffer_pool_cleanup();

    mu_assert(buffer_pool_count() == 0, "buffer_pool_count should be 0 after cleanup");
    mu_assert(buffer_pool_bufsize() == 0, "buffer_pool_bufsize should be 0 after cleanup");
}

MU_TEST(test_data_access) {
    bool success = buffer_pool_init(3, 1048576);

    mu_check(success);

    void* buf0 = buffer_pool_data(0);
    void* buf1 = buffer_pool_data(1);
    void* buf2 = buffer_pool_data(2);
    void* invalid = buffer_pool_data(3);

    mu_assert(buf0 != NULL, "buffer_pool_data(0) should not be NULL");
    mu_assert(buf1 != NULL, "buffer_pool_data(1) should not be NULL");
    mu_assert(buf2 != NULL, "buffer_pool_data(2) should not be NULL");
    mu_assert(invalid == NULL, "buffer_pool_data(3) should be NULL");

    buffer_pool_cleanup();
}

MU_TEST(test_write_and_read) {
    bool success = buffer_pool_init(4, 1048576);
    
    mu_check(success);
    
    char* buf = (char*)buffer_pool_data(0);
    strcpy(buf, "hello");

    mu_assert(strcmp(buf, "hello") == 0, "buffer_pool_data(0) should contains 'hello'");

    buffer_pool_cleanup();
}

MU_TEST_SUITE(test_suite) {
    MU_RUN_TEST(test_init_and_cleanup);
    MU_RUN_TEST(test_data_access);
    MU_RUN_TEST(test_write_and_read);
}

int main(void) {
    MU_RUN_SUITE(test_suite);
    MU_REPORT();
    return MU_EXIT_CODE;
}
