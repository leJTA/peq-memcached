#include "3q_history_buffer.h"
#include "third_party/minunit/minunit.h"

MU_TEST(test_create_empty_buffer) {
   history_buffer* history = create_history_buffer(5);
   
   mu_assert(history_buffer_is_empty(history), "Buffer should be empty");
   mu_assert(history->head == NULL, "Head should be NULL");
   mu_assert(history->tail == NULL, "Tail should be NULL");
   
   destroy_history_buffer(history);
}

MU_TEST(test_create_buffer_with_invalid_capacity) {
   history_buffer* history = create_history_buffer(0);

   mu_assert(history == NULL, "Buffer of invalid capacity should be null");

   destroy_history_buffer(history);
}

MU_TEST(test_enqueue_one_element) {
   history_buffer* history = create_history_buffer(5);
   
   history_buffer_enqueue(history, "hello", 5);
   mu_assert_int_eq(1, history->size);
   mu_assert(history->head == history->tail, "Head and tail should be equal");
   mu_assert(history_buffer_contains(history, "hello", 5), "Should contain 'hello'");
   
   destroy_history_buffer(history);
}

MU_TEST(test_dequeue_element) {
   history_buffer* history = create_history_buffer(5);
   
   history_buffer_enqueue(history, "hello", 5);
   history_buffer_dequeue(history);
   
   mu_assert(!history_buffer_contains(history, "hello", 5), "Should not contain 'hello'");
   mu_assert(history_buffer_is_empty(history), "Buffer should be empty");
   mu_assert(history->head == NULL, "Head should be NULL");
   mu_assert(history->tail == NULL, "Tail should be NULL");
   
   destroy_history_buffer(history);
}

MU_TEST(test_fill_buffer) {
   history_buffer* history = create_history_buffer(5);
   
   history_buffer_enqueue(history, "hello", 5);
   history_buffer_enqueue(history, "world", 5);
   history_buffer_enqueue(history, "foo", 3);
   history_buffer_enqueue(history, "bar", 3);
   history_buffer_enqueue(history, "bazz", 4);
   
   mu_assert_int_eq(5, history->size);
   mu_assert(history->tail->next == NULL, "Tail next should be NULL");
   mu_assert(history_buffer_contains(history, "bazz", 4), "Should contain 'bazz'");
   
   destroy_history_buffer(history);
}

MU_TEST(test_overflow_buffer) {
   history_buffer* history = create_history_buffer(5);
   
   history_buffer_enqueue(history, "hello", 5);
   history_buffer_enqueue(history, "world", 5);
   history_buffer_enqueue(history, "foo", 3);
   history_buffer_enqueue(history, "bar", 3);
   history_buffer_enqueue(history, "bazz", 4);
   history_buffer_enqueue(history, "toto", 4); 
   
   mu_assert(!history_buffer_contains(history, "hello", 5), "Should not contain 'hello'");
   mu_assert_int_eq(5, history->size);
   
   destroy_history_buffer(history);
}

MU_TEST(test_remove_element) {
   history_buffer* history = create_history_buffer(5);
   
   history_buffer_enqueue(history, "hello", 5);
   history_buffer_enqueue(history, "world", 5);
   history_buffer_enqueue(history, "foo", 3);
   history_buffer_enqueue(history, "bar", 3);
   history_buffer_enqueue(history, "bazz", 4);
   
   history_buffer_remove(history, "foo", 3);
   
   mu_assert_int_eq(4, history->size);
   mu_assert(!history_buffer_contains(history, "foo", 3), "Should not contain 'foo'");
   
   destroy_history_buffer(history);
}

MU_TEST_SUITE(test_suite) {
   MU_RUN_TEST(test_create_empty_buffer);
   MU_RUN_TEST(test_create_buffer_with_invalid_capacity);
   MU_RUN_TEST(test_enqueue_one_element);
   MU_RUN_TEST(test_dequeue_element);
   MU_RUN_TEST(test_fill_buffer);
   MU_RUN_TEST(test_overflow_buffer);
   MU_RUN_TEST(test_remove_element);
}

int main() {
   MU_RUN_SUITE(test_suite);
   MU_REPORT();
   return 0;
}