#ifndef TEST_H
#define TEST_H

#include <stdio.h>

int TOTAL_PASS = 1;
int SUITE_PASS = 0;
int TEST_PASS  = 0;

static int __started = 0;

static inline void __test_failed(void);
static inline void __FAIL(void);
static inline void __PASS(void);
static inline void __setup_test(void);
static inline int __finish(void);
static inline void __TEST(const char *s);

#define SKIP_REMAINING (!SUITE_PASS)
#define FAIL __FAIL()
#define PASS __PASS()
#define START  printf("---- RUNNING TEST SUITES ----\n")
#define FINISH __finish()
#define SUITE(n) void test_ ## n (void)
#define RUN(n) do { \
	__setup_suite(); \
	printf("SUITE: %s\n", #n); \
	test_ ## n (); \
} while (0)
#define TEST(s) __TEST(s)

/**********************************************************/

static void DEBUG_TEST_ENV(void)
{
	printf("\n");
	printf("---------------------------------------------\n");
	printf("Total/Suite/Test: %i/%i/%i\n",
	       TOTAL_PASS, SUITE_PASS, TEST_PASS);
	printf("---------------------------------------------\n");
}

static inline void __test_failed(void)
{
	TOTAL_PASS = SUITE_PASS = TEST_PASS = 0;
}

static inline void __FAIL(void)
{
	__test_failed();
	printf("FAIL\n");
}

static inline void __PASS(void)
{
	if (!TEST_PASS) { return; }
	printf("PASS\n");
}

static inline void __setup_test(void)
{
	if (__started && TEST_PASS) { PASS; }
	TEST_PASS = __started = 1;
	//DEBUG_TEST_ENV();
}

static inline void __setup_suite(void)
{
	if (__started && TEST_PASS) { PASS; }
	SUITE_PASS = __started = 1;
}

static inline int __finish(void)
{
	__setup_test();
	return TOTAL_PASS ? 0 : 1;
}

static inline void __TEST(const char *s)
{
	__setup_test();
	printf("  - Testing %s... ", s);
	if (!SUITE_PASS) { printf("SKIP\n"); }
}

/**********************************************************/

#define ASSERTION_FAILED_MSG "Assertion failed"

static void assert_int_equals(const char *s, int expected, int actual)
{
	if (!TEST_PASS) { return; }
	if (expected == actual) { return; }

	__test_failed();
	printf("FAIL: %i != %i\n", expected, actual);
}

static void assert_str_equals(const char *s, const char *expected, const char *actual)
{
	if (!TEST_PASS) { return; }
	if (strcmp(expected, actual) == 0) { return; }

	__test_failed();
	printf("FAIL: %s != %s\n", expected, actual);
}


#endif /* TEST_H */
