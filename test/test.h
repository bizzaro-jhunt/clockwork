#ifndef TEST_H
#define TEST_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
void assert_signed_equals(const char *s, signed long int expected, signed long int actual);

void assert_ptr(const char *s, const void *expected, const void *actual);

void assert_int_equals(const char *s, int expected, int actual);
void assert_int_not_equal(const char *s, int unexpected, int actual);
void assert_int_greater_than(const char *s, int actual, int threshold);
void assert_int_greater_than_or_equal(const char *s, int actual, int threshold);
void assert_int_less_than(const char *s, int actual, int threshold);
void assert_int_less_than_or_equal(const char *s, int actual, int threshold);

void assert_str_equals(const char *s, const char *expected, const char *actual);
void assert_str_not_equal(const char *s, const char *unexpected, const char *actual);

#endif /* TEST_H */
