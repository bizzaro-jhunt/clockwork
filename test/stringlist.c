#include <stdio.h>

#include "test.h"
#include "../stringlist.h"

void test_stringlist_init()
{
	stringlist *sl;

	sl = stringlist_new();

	test("stringlist: Initialization Routines");
	assert_not_null("stringlist_new returns a non-null pointer", sl);
	assert_int_equals("a new stringlist is empty", sl->num, 0);
	assert_int_not_equal("a new stringlist has extra capacity", sl->len, 0);

	stringlist_free(sl);
}

void test_stringlist_basic_add_remove_search()
{
	stringlist *sl;

	sl = stringlist_new();

	test("stringlist: Basic Add/Remove/Search");
	assert_not_null("have a non-null stringlist", sl);

	assert_int_equals("add 'first string' to string list", stringlist_add(sl, "first string"), 0);
	assert_int_equals("add 'second string' to string list", stringlist_add(sl, "second string"), 0);

	assert_int_equals("stringlist has 2 strings", sl->num, 2);
	assert_str_equals("strings[0] is 'first string'", sl->strings[0], "first string");
	assert_str_equals("strings[1] is 'second string'", sl->strings[1], "second string");
	assert_null("strings[2] is NULL", sl->strings[2]);

	assert_int_equals("search for 'first string' succeeds", stringlist_search(sl, "first string"), 0);
	assert_int_equals("search for 'second string' succeeds", stringlist_search(sl, "second string"), 0);
	assert_int_not_equal("search for 'no such string' fails", stringlist_search(sl, "no such string"), 0);

	stringlist_free(sl);
}

void test_stringlist_expansion()
{
	stringlist *sl;
	size_t max, i, len;
	char buf[32];
	char assertion[128];

	sl = stringlist_new();
	test("stringlist: Automatic List Expansion");
	assert_not_null("have a non-null stringlist", sl);

	max = sl->len;
	for (i = 0; i < max * 2 + 1; i++) {
		snprintf(buf, 32, "string%u", i);
		snprintf(assertion, 128, "add 'string%u' to stringlist", i);
		assert_int_equals(assertion, stringlist_add(sl, buf), 0);

		snprintf(assertion, 128, "stringlist should have %u strings", i + 1);
		assert_int_equals(assertion, sl->num, i + 1);
	}

	assert_int_not_equal("sl->len should have changed", sl->len, max);

	stringlist_free(sl);
}

void test_suite_stringlist()
{
	test_stringlist_init();
	test_stringlist_basic_add_remove_search();
	test_stringlist_expansion();
}
