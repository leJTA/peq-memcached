#include "3q_history_buffer.h"
#include "third_party/minunit/minunit.h"

#ifdef UNIT_TESTING
void* history_buffer_tail();
void* history_buffer_head();
#endif // UNIT_TESTING

MU_TEST(test_create_empty_buffer) {
   history_buffer_init(5);
   
   mu_assert_int_eq(5, history_buffer_capacity());
   mu_assert_int_eq(32 + 5 * (24 + 255), history_buffer_max_mem_usage());
   mu_assert(history_buffer_is_empty(), "Buffer should be empty");
   mu_assert(history_buffer_tail() == NULL, "Head should be NULL");
   mu_assert(history_buffer_head() == NULL, "Tail should be NULL");
   
   history_buffer_cleanup();
}

MU_TEST(test_create_buffer_with_invalid_capacity) {
   bool success = history_buffer_init(0);

   mu_assert_int_eq(0, history_buffer_max_mem_usage());
   mu_assert(!success, "Allocation of buffer with invalid capacity should fail");

   history_buffer_cleanup();
}

MU_TEST(test_enqueue_one_element) {
   history_buffer_init(5);
   
   history_buffer_enqueue("hello", 5);
   mu_assert_int_eq(1, history_buffer_size());
   mu_assert(history_buffer_tail() == history_buffer_head(), "Head and tail should be equal");
   mu_assert(history_buffer_contains("hello", 5), "Should contain 'hello'");
   
   history_buffer_cleanup();
}

MU_TEST(test_dequeue_element) {
   history_buffer_init(5);
   
   history_buffer_enqueue("hello", 5);
   history_buffer_dequeue();
   
   mu_assert(!history_buffer_contains("hello", 5), "Should not contain 'hello'");
   mu_assert(history_buffer_is_empty(), "Buffer should be empty");
   mu_assert(history_buffer_head() == NULL, "Head should be NULL");
   mu_assert(history_buffer_tail() == NULL, "Tail should be NULL");
   
   history_buffer_cleanup();
}

MU_TEST(test_fill_buffer) {
   history_buffer_init(5);
   
   history_buffer_enqueue("hello", 5);
   history_buffer_enqueue("world", 5);
   history_buffer_enqueue("foo", 3);
   history_buffer_enqueue("bar", 3);
   history_buffer_enqueue("bazz", 4);
   
   mu_assert_int_eq(5, history_buffer_size());
   mu_assert(history_buffer_contains("bazz", 4), "Should contain 'bazz'");
   
   history_buffer_cleanup();
}

MU_TEST(test_overflow_buffer) {
   history_buffer_init(5);
   
   history_buffer_enqueue("hello", 5);
   history_buffer_enqueue("world", 5);
   history_buffer_enqueue("foo", 3);
   history_buffer_enqueue("bar", 3);
   history_buffer_enqueue("bazz", 4);
   history_buffer_enqueue("toto", 4); 
   
   mu_assert(!history_buffer_contains("hello", 5), "Should not contain 'hello'");
   mu_assert_int_eq(5, history_buffer_size());
   
   history_buffer_cleanup();
}

MU_TEST(test_remove_element) {
   history_buffer_init(5);
   
   history_buffer_enqueue("hello", 5);
   history_buffer_enqueue("world", 5);
   history_buffer_enqueue("foo", 3);
   history_buffer_enqueue("bar", 3);
   history_buffer_enqueue("bazz", 4);
   
   bool removed = history_buffer_remove("foo", 3);
   
   mu_check(removed);
   mu_assert_int_eq(4, history_buffer_size());
   mu_assert(!history_buffer_contains("foo", 3), "Should not contain 'foo'");
   
   history_buffer_cleanup();
}

MU_TEST(test_remove_non_existent_element) {
   history_buffer_init(4);
   
   history_buffer_enqueue("hello", 5);
   history_buffer_enqueue("world", 5);
   history_buffer_enqueue("foo", 3);
   history_buffer_enqueue("bar", 3);
   
   bool removed = history_buffer_remove("bazz", 4);
   mu_check(!removed);
   mu_assert(history_buffer_size() == 4, "Removing non existent element should not modify the size");

   history_buffer_cleanup();
}

MU_TEST(test_reinsert_element) {
   history_buffer_init(5);

   history_buffer_enqueue("hello", 5);
   bool removed = history_buffer_remove("hello", 5);
   history_buffer_enqueue("world", 5);
   history_buffer_enqueue("foo", 3);
   history_buffer_enqueue("hello", 5);

   mu_check(removed);
   mu_assert_int_eq(3, history_buffer_size());
   mu_assert(history_buffer_contains("hello", 5), "Should contain 'hello'");

   history_buffer_cleanup();
}

MU_TEST_SUITE(test_suite) {
   MU_RUN_TEST(test_create_empty_buffer);
   MU_RUN_TEST(test_create_buffer_with_invalid_capacity);
   MU_RUN_TEST(test_enqueue_one_element);
   MU_RUN_TEST(test_dequeue_element);
   MU_RUN_TEST(test_fill_buffer);
   MU_RUN_TEST(test_overflow_buffer);
   MU_RUN_TEST(test_remove_element);
   MU_RUN_TEST(test_remove_non_existent_element);
   MU_RUN_TEST(test_reinsert_element);
}

int main() {
   MU_RUN_SUITE(test_suite);
   MU_REPORT();
   return MU_EXIT_CODE;
}