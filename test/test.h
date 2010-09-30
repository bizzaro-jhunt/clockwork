#ifndef TEST_H
#define TEST_H

inline void test(const char *s);
int test_status(void);
int test_setup(int argc, char **argv);

/** ASSERTIONS **/

void assert_fail(const char *s);
void assert_pass(const char *s);
void assert_true(const char *s, int value);
void assert_false(const char *s, int value);
void assert_not_null(const char *s, void *ptr);
void assert_null(const char *s, void *ptr);
void assert_int_equals(const char *s, int expected, int actual);
void assert_int_not_equal(const char *s, int unexpected, int actual);
void assert_str_equals(const char *s, const char *expected, const char *actual);
void assert_str_not_equal(const char *s, const char *unexpected, const char *actual);

#endif /* TEST_H */
