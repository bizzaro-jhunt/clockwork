#include <stdio.h>
#include <string.h>

#include "test.h"
#include "../stringlist.h"

stringlist* setup_stringlist(const char *s1, const char *s2, const char *s3)
{
	stringlist *sl;

	sl = stringlist_new(NULL);
	assert_not_null("stringlist_new returns a non-null pointer", sl);
	assert_int_equals("adding the s1 string to the list", stringlist_add(sl, s1), 0);
	assert_int_equals("adding the s2 string to the list", stringlist_add(sl, s2), 0);
	assert_int_equals("adding the s3 string to the list", stringlist_add(sl, s3), 0);

	return sl;
}

void test_stringlist_init()
{
	stringlist *sl;

	sl = stringlist_new(NULL);

	test("stringlist: Initialization Routines");
	assert_not_null("stringlist_new returns a non-null pointer", sl);
	assert_int_equals("a new stringlist is empty", sl->num, 0);
	assert_int_not_equal("a new stringlist has extra capacity", sl->len, 0);

	stringlist_free(sl);
}

void test_stringlist_init_with_data()
{
	stringlist *sl;
	char *data[32];
	char buf[32];
	size_t i;

	for (i = 0; i < 32; i++) {
		snprintf(buf, 32, "data%u", i);
		data[i] = strdup(buf);
	}
	data[32] = NULL;

	sl = stringlist_new(data);
	test("stringlist: Initialization (with data)");
	assert_int_equals("stringlist should have 32 strings", sl->num, 32);
	assert_int_greater_than("stringlist should have more than 32 slots", sl->len, 32);
	assert_str_equals("spot-check strings[0]", sl->strings[0], "data0");
	assert_str_equals("spot-check strings[14]", sl->strings[14], "data14");
	assert_str_equals("spot-check strings[28]", sl->strings[28], "data28");
	assert_str_equals("spot-check strings[31]", sl->strings[31], "data31");
	assert_null("strings[32] should be a NULL terminator", sl->strings[32]);
}

void test_stringlist_basic_add_remove_search()
{
	stringlist *sl;

	sl = stringlist_new(NULL);

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

	sl = stringlist_new(NULL);
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

void test_stringlist_diff()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";
	const char *d = "david";
	const char *e = "ethan";

	stringlist *sl1, *sl2;

	sl1 = setup_stringlist(b, c, a);
	sl2 = setup_stringlist(b, c, a);

	test("stringlist: Diff");
	assert_int_not_equal("sl1 and sl2 are equivalent", stringlist_diff(sl1, sl2), 0);

	stringlist_add(sl2, d);
	assert_int_equals("after addition of 'david' to sl2, sl1 and sl2 differ", stringlist_diff(sl1, sl2), 0);

	stringlist_add(sl1, e);
	assert_int_equals("after addition of 'ethan' to sl1, sl1 and sl2 differ", stringlist_diff(sl1, sl2), 0);
}

void test_suite_stringlist()
{
	test_stringlist_init();
	test_stringlist_init_with_data();
	test_stringlist_basic_add_remove_search();
	test_stringlist_expansion();
	test_stringlist_qsort();
	test_stringlist_uniq();
	test_stringlist_uniq_already();
	test_stringlist_diff();
}
