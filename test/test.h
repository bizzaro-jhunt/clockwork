#ifndef TEST_H
#define TEST_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef void (*test_suite_f)(void);

struct test_suite {
	const char    *name;
	test_suite_f   runner;
	int            active;
};

int add_test_suite(const char *name, test_suite_f runner, int active);
#define TEST_SUITE(x) extern void test_suite_ ## x(); add_test_suite(#x, test_suite_ ## x, 0)

int activate_test(const char *name);
int run_active_tests(void);
int run_all_tests(void);

void test(const char *s);
int test_status(void);
int test_setup(int argc, char **argv);

/** ASSERTIONS **/

void assert_fail(const char *s);
void assert_pass(const char *s);
void assert_true(const char *s, int value);
void assert_false(const char *s, int value);
void assert_not_null(const char *s, const void *ptr);
void assert_null(const char *s, const void *ptr);

void assert_unsigned_equals(const char *s, unsigned long int, unsigned long int actual);
#define assert_unsigned_equal assert_unsigned_equals
#define assert_unsigned_eq    assert_unsigned_eq

void assert_signed_equals(const char *s, signed long int expected, signed long int actual);
#define assert_signed_equal assert_signed_equals
#define assert_signed_eq    assert_signed_equals

void assert_ptr(const char *s, const void *expected, const void *actual);
#define assert_ptr_eq     assert_ptr
#define assert_ptr_equal  assert_ptr
#define assert_ptr_equals assert_ptr
void assert_ptr_ne(const char *s, const void *unexpected, const void *actual);
#define assert_ptr_not_equal  assert_ptr_ne
#define assert_ptr_not_equals assert_ptr_ne

void assert_int_equals(const char *s, int expected, int actual);
#define assert_int_equal assert_int_equals
#define assert_int_eq    assert_int_equals

void assert_int_not_equal(const char *s, int unexpected, int actual);
#define assert_int_not_equals assert_int_not_equal
#define assert_int_ne         assert_int_not_equal

void assert_int_greater_than(const char *s, int actual, int threshold);
#define assert_int_gt assert_int_greater_than

void assert_int_greater_than_or_equal(const char *s, int actual, int threshold);
#define assert_int_gte assert_int_greater_than_or_equal

void assert_int_less_than(const char *s, int actual, int threshold);
#define assert_int_lt assert_int_less_than

void assert_int_less_than_or_equal(const char *s, int actual, int threshold);
#define assert_int_lte assert_int_less_than_or_equal

void assert_str_equals(const char *s, const char *expected, const char *actual);
#define assert_str_equal assert_str_equals
#define assert_str_eq    assert_str_equals

void assert_str_not_equal(const char *s, const char *unexpected, const char *actual);
#define assert_str_not_equals assert_str_not_equal
#define assert_str_ne         assert_str_not_equal

#endif /* TEST_H */
