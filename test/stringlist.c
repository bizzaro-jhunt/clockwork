#include <stdio.h>
#include <string.h>

#include "test.h"
#include "../stringlist.h"

stringlist* setup_stringlist(const char *s1, const char *s2, const char *s3)
{
	stringlist *sl;

	sl = stringlist_new();
	assert_not_null("stringlist_new returns a non-null pointer", sl);
	assert_int_equals("adding the s1 string to the list", stringlist_add(sl, s1), 0);
	assert_int_equals("adding the s2 string to the list", stringlist_add(sl, s2), 0);
	assert_int_equals("adding the s3 string to the list", stringlist_add(sl, s3), 0);

	return sl;
}

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

	assert_int_equals("removal of 'first string' succeeds", stringlist_remove(sl, "first string"), 0);
	assert_int_equals("stringlist now has 1 string", sl->num, 1);
	assert_int_not_equal("search for 'first string' fails", stringlist_search(sl, "first string"), 0);
	assert_int_equals("search for 'second string' still succeeds", stringlist_search(sl, "second string"), 0);

	assert_str_equals("strings[0] is 'second string'", sl->strings[0], "second string");
	assert_null("strings[1] is NULL", sl->strings[1]);

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

void test_stringlist_remove_nonexistent()
{
	const char *tomato = "tomato";

	stringlist *sl;
	sl = setup_stringlist("apple", "pear", "banana");

	test("stringlist: Remove non-existent");
	assert_int_not_equal("stringlist does not contain 'tomato'", stringlist_search(sl, tomato), 0);
	assert_int_not_equal("removal of 'tomato' from stringlist fails", stringlist_remove(sl, tomato), 0);

	stringlist_free(sl);
}

void test_stringlist_qsort()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	stringlist *sl;
	test("stringlist: Sorting");
	sl = setup_stringlist(b, c, a);
	assert_str_equals("pre-sort strings[0] is 'bob'", sl->strings[0], "bob");
	assert_str_equals("pre-sort strings[1] is 'candace'", sl->strings[1], "candace");
	assert_str_equals("pre-sort strings[2] is 'alice'", sl->strings[2], "alice");
	assert_null("pre-sort ASC strings[3] is NULL", sl->strings[3]);

	stringlist_sort(sl, STRINGLIST_SORT_ASC);
	assert_str_equals("post-sort ASC strings[0] is 'alice'", sl->strings[0], "alice");
	assert_str_equals("post-sort ASC strings[1] is 'bob'", sl->strings[1], "bob");
	assert_str_equals("post-sort ASC strings[2] is 'candace'", sl->strings[2], "candace");
	assert_null("post-sort ASC strings[3] is NULL", sl->strings[3]);

	stringlist_sort(sl, STRINGLIST_SORT_DESC);
	assert_str_equals("post-sort ASC strings[0] is 'candace'", sl->strings[0], "candace");
	assert_str_equals("post-sort ASC strings[1] is 'bob'", sl->strings[1], "bob");
	assert_str_equals("post-sort ASC strings[2] is 'alice'", sl->strings[2], "alice");
	assert_null("post-sort ASC strings[3] is NULL", sl->strings[3]);
}

void test_stringlist_uniq()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	stringlist *sl;

	sl = setup_stringlist(b, c, a);
	stringlist_add(sl, b);
	stringlist_add(sl, b);
	stringlist_add(sl, c);

	test("stringlist: Uniq");
	assert_int_equals("pre-uniq stringlist has 6 strings", sl->num, 6);
	assert_str_equals("pre-uniq strings[0] is 'bob'",     sl->strings[0], "bob");
	assert_str_equals("pre-uniq strings[1] is 'candace'", sl->strings[1], "candace");
	assert_str_equals("pre-uniq strings[2] is 'alice'",   sl->strings[2], "alice");
	assert_str_equals("pre-uniq strings[3] is 'bob'",     sl->strings[3], "bob");
	assert_str_equals("pre-uniq strings[4] is 'bob'",     sl->strings[4], "bob");
	assert_str_equals("pre-uniq strings[5] is 'candace'", sl->strings[5], "candace");
	assert_null("pre-uniq strings[6] is NULL", sl->strings[6]);

	stringlist_uniq(sl);
	assert_int_equals("post-uniq stringlist has 3 strings", sl->num, 3);
	assert_str_equals("post-uniq strings[0] is 'alice'",   sl->strings[0], "alice");
	assert_str_equals("post-uniq strings[1] is 'bob'",     sl->strings[1], "bob");
	assert_str_equals("post-uniq strings[2] is 'candace'", sl->strings[2], "candace");
	assert_null("post-uniq strings[3] is NULL", sl->strings[3]);
}

void test_stringlist_uniq_already()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	stringlist *sl;

	sl = setup_stringlist(b, c, a);

	test("stringlist: Uniq (no changes needed)");
	assert_int_equals("pre-uniq stringlist has 3 strings", sl->num, 3);
	assert_str_equals("pre-uniq strings[0] is 'bob'",     sl->strings[0], "bob");
	assert_str_equals("pre-uniq strings[1] is 'candace'", sl->strings[1], "candace");
	assert_str_equals("pre-uniq strings[2] is 'alice'",   sl->strings[2], "alice");
	assert_null("pre-uniq strings[3] is NULL", sl->strings[3]);

	stringlist_uniq(sl);
	assert_int_equals("post-uniq stringlist has 3 strings", sl->num, 3);
	assert_str_equals("post-uniq strings[0] is 'alice'",   sl->strings[0], "alice");
	assert_str_equals("post-uniq strings[1] is 'bob'",     sl->strings[1], "bob");
	assert_str_equals("post-uniq strings[2] is 'candace'", sl->strings[2], "candace");
	assert_null("post-uniq strings[3] is NULL", sl->strings[3]);
}

void test_suite_stringlist()
{
	test_stringlist_init();
	test_stringlist_basic_add_remove_search();
	test_stringlist_expansion();
	test_stringlist_qsort();
	test_stringlist_uniq();
	test_stringlist_uniq_already();
}
