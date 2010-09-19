#ifndef TEST_H
#define TEST_H

#include <stdio.h>

int TEST_PRINT_PASS = 0;
int TEST_PRINT_FAIL = 1;

int __STATUS = 0;
int __TESTS = 0;
int __ASSERTIONS = 0;
int __FAILURES = 0;

static inline void __test_failed(void);
static inline void test(const char *s);
static inline int  test_status(void);

/**********************************************************/

static inline void __test_failed(void)
{
	++__FAILURES;
	__STATUS = 0;
}

static inline int test_status(void)
{
	printf("\n"
	       "--------------------\n"
	       "TEST RESULTS SUMMARY\n"
	       "--------------------\n");
	printf("%4i test(s)\n"
	       "%4i assertion(s)\n"
	       "\n"
	       "%4i FAILURE(S)\n",
	       __TESTS, __ASSERTIONS, __FAILURES);

	return __FAILURES;
}

static inline void test(const char *s)
{
	__STATUS = 1;
	++__TESTS;
	printf("%s\n", s);
}

/**********************************************************/

static void assert_fail(const char *s)
{
	++__ASSERTIONS;
	__test_failed();
	if (TEST_PRINT_FAIL) { printf(" - %s: FAIL\n", s); }
}

static void assert_pass(const char *s)
{
	++__ASSERTIONS;
	if (TEST_PRINT_PASS) { printf(" - %s: PASS\n", s); }
}

static void assert_true(const char *s, int value)
{
	++__ASSERTIONS;
	(value ? assert_pass(s) : assert_fail(s));
}

static void assert_false(const char *s, int value)
{
	assert_true(s, !value);
}

static void assert_not_null(const char *s, void *ptr)
{
	assert_true(s, ptr != NULL);
}

static void assert_null(const char *s, void *ptr)
{
	assert_true(s, ptr == NULL);
}

static void assert_int_equals(const char *s, int expected, int actual)
{
	++__ASSERTIONS;
	if (expected == actual) {
		if (TEST_PRINT_PASS) { printf(" - %s: PASS\n", s); }
	} else {
		__test_failed();
		if (TEST_PRINT_FAIL) {
			printf(" - %s: FAIL: %i != %i\n", s, expected, actual);
		}
	}
}

static void assert_str_equals(const char *s, const char *expected, const char *actual)
{
	++__ASSERTIONS;
	if (strcmp(expected, actual) == 0) {
		if (TEST_PRINT_PASS) { printf(" - %s: PASS\n", s); }
	} else {
		__test_failed();
		if (TEST_PRINT_FAIL) {
			printf(" - %s: FAIL: %s != %s\n", s, expected, actual);
		}
	}
}


#endif /* TEST_H */
