/*
  Copyright 2011 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "test.h"
#include "assertions.h"
#include "../../stringlist.h"

static int setup_list_item(struct stringlist *list, const char *item)
{
	char buf[128];
	int ret;

	snprintf(buf, 128, "adding item '%s' to list", item);
	assert_int_eq(buf, 0, ret = stringlist_add(list, item));
	return ret;
}

static struct stringlist* setup_list(const char *first, ...)
{
	va_list args;
	struct stringlist *list;
	const char *item;

	list = stringlist_new(NULL);
	assert_not_null("stringlist_new returns a non-NULL pointer", list);

	if (setup_list_item(list, first) != 0) { return list; }

	va_start(args, first);
	while ( (item = va_arg(args, const char*)) != NULL) {
		if (setup_list_item(list, item) != 0) { break; }
	}
	va_end(args);
	return list;
}

/**********************************************************************/

void test_stringlist_init()
{
	struct stringlist *sl;

	sl = stringlist_new(NULL);

	test("stringlist: Initialization Routines");
	assert_not_null("stringlist_new returns a non-null pointer", sl);
	assert_int_eq("a new stringlist is empty", sl->num, 0);
	assert_int_ne("a new stringlist has extra capacity", sl->len, 0);

	stringlist_free(sl);
}

void test_stringlist_init_with_data()
{
	struct stringlist *sl;
	char *data[33];
	char buf[32];
	size_t i;

	for (i = 0; i < 32; i++) {
		snprintf(buf, 32, "data%u", i);
		data[i] = strdup(buf);
	}
	data[32] = NULL;

	sl = stringlist_new(data);
	test("stringlist: Initialization (with data)");
	assert_int_eq("stringlist should have 32 strings", sl->num, 32);
	assert_int_gt("stringlist should have more than 32 slots", sl->len, 32);
	assert_str_eq("spot-check strings[0]", sl->strings[0], "data0");
	assert_str_eq("spot-check strings[14]", sl->strings[14], "data14");
	assert_str_eq("spot-check strings[28]", sl->strings[28], "data28");
	assert_str_eq("spot-check strings[31]", sl->strings[31], "data31");
	assert_null("strings[32] should be a NULL terminator", sl->strings[32]);

	stringlist_free(sl);
	for (i = 0; i < 32; i++) {
		free(data[i]);
	}
}

void test_stringlist_dup()
{
	struct stringlist *orig, *dup;
	char *data[5];
	char buf[32];
	size_t i;

	for (i = 0; i < 4; i++) {
		snprintf(buf, 32, "data%u", i);
		data[i] = strdup(buf);
	}
	data[4] = NULL;

	orig = stringlist_new(data);
	dup  = stringlist_dup(orig);

	assert_int_eq("original stringlist has 4 strings", orig->num, 4);
	assert_int_eq("duplicate stringlist has 4 strings", orig->num, 4);

	assert_str_eq("original->strings[0]",  orig->strings[0], "data0");
	assert_str_eq("duplicate->strings[0]", dup->strings[0], "data0");

	assert_str_eq("original->strings[1]",  orig->strings[1], "data1");
	assert_str_eq("duplicate->strings[1]", dup->strings[1], "data1");

	assert_str_eq("original->strings[2]",  orig->strings[2], "data2");
	assert_str_eq("duplicate->strings[2]", dup->strings[2], "data2");

	assert_str_eq("original->strings[3]",  orig->strings[3], "data3");
	assert_str_eq("duplicate->strings[3]", dup->strings[3], "data3");

	stringlist_free(orig);
	stringlist_free(dup);

	for (i = 0; i < 4; i++) {
		free(data[i]);
	}
}

void test_stringlist_basic_add_remove_search()
{
	struct stringlist *sl;

	sl = stringlist_new(NULL);

	test("stringlist: Basic Add/Remove/Search");
	assert_not_null("have a non-null stringlist", sl);

	assert_int_eq("add 'first string' to string list", stringlist_add(sl, "first string"), 0);
	assert_int_eq("add 'second string' to string list", stringlist_add(sl, "second string"), 0);

	assert_stringlist(sl, "sl", 2, "first string", "second string");

	assert_int_eq("search for 'first string' succeeds", stringlist_search(sl, "first string"), 0);
	assert_int_eq("search for 'second string' succeeds", stringlist_search(sl, "second string"), 0);
	assert_int_ne("search for 'no such string' fails", stringlist_search(sl, "no such string"), 0);

	assert_int_eq("removal of 'first string' succeeds", stringlist_remove(sl, "first string"), 0);
	assert_stringlist(sl, "sl", 1, "second string");

	assert_int_eq("search for 'second string' still succeeds", stringlist_search(sl, "second string"), 0);

	stringlist_free(sl);
}

void test_stringlist_add_all()
{
	struct stringlist *sl1, *sl2;

	sl1 = setup_list("lorem", "ipsum", "dolor", NULL);
	sl2 = setup_list("sit", "amet", "consectetur", NULL);

	test("stringlist: Combination of string lists");
	assert_stringlist(sl1, "pre-combine sl1", 3, "lorem", "ipsum", "dolor");
	assert_stringlist(sl2, "pre-combine sl2", 3, "sit", "amet", "consectetur");

	assert_int_eq("combine sl1 and sl2 successfully", stringlist_add_all(sl1, sl2), 0);
	assert_stringlist(sl1, "post-combine sl1", 6, "lorem", "ipsum", "dolor", "sit", "amet", "consectetur");
	assert_stringlist(sl2, "post-combine sl2", 3, "sit", "amet", "consectetur");

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_add_all_with_expansion()
{
	struct stringlist *sl1, *sl2;
	int total = 0;
	int capacity = 0;

	test("stringlist: add_all with capacity expansion");
	sl1 = setup_list("a", "b", "c", "d", "e", "f", "g",
	                 "h", "i", "j", "k", "l", "m", "n", NULL);
	assert_stringlist(sl1, "sl1", 14,
		"a", "b", "c", "d", "e", "f", "g",
		"h", "i", "j", "k", "l", "m", "n");

	sl2 = setup_list("o", "p", "q", NULL);
	assert_stringlist(sl2, "sl2", 3, "o", "p", "q");

	total = sl1->num + sl2->num;
	capacity = sl1->len;
	assert_int_gt("combined len should be > capacity", total, capacity);
	assert_int_eq("add_all succeeds", stringlist_add_all(sl1, sl2), 0);
	assert_stringlist(sl1, "combined", 17,
		"a", "b", "c", "d", "e", "f", "g",
		"h", "i", "j", "k", "l", "m", "n",
		"o", "p", "q");
	assert_int_ge("new capacity >= old capacity", sl1->len, capacity);

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_remove_all()
{
	struct stringlist *sl1, *sl2;

	sl1 = setup_list("lorem", "ipsum", "dolor", NULL);
	sl2 = setup_list("ipsum", "dolor", "sit", NULL);

	test("stringlist: Removal of string lists");
	assert_stringlist(sl1, "pre-remove sl1", 3, "lorem", "ipsum", "dolor");
	assert_stringlist(sl2, "pre-remove sl2", 3, "ipsum", "dolor", "sit");

	assert_int_eq("remove sl2 from sl1 successfully", stringlist_remove_all(sl1, sl2), 0);
	assert_stringlist(sl1, "post-remove sl1", 1, "lorem");
	assert_stringlist(sl2, "post-remove sl2", 3, "ipsum", "dolor", "sit");

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_expansion()
{
	struct stringlist *sl;
	size_t max, i;
	char buf[32];
	char assertion[128];

	sl = stringlist_new(NULL);
	test("stringlist: Automatic List Expansion");
	assert_not_null("have a non-null stringlist", sl);

	max = sl->len;
	for (i = 0; i < max * 2 + 1; i++) {
		snprintf(buf, 32, "string%u", i);
		snprintf(assertion, 128, "add 'string%u' to stringlist", i);
		assert_int_eq(assertion, stringlist_add(sl, buf), 0);

		snprintf(assertion, 128, "stringlist should have %u strings", i + 1);
		assert_int_eq(assertion, sl->num, i + 1);
	}

	assert_int_ne("sl->len should have changed", sl->len, max);

	stringlist_free(sl);
}

void test_stringlist_remove_nonexistent()
{
	const char *tomato = "tomato";

	struct stringlist *sl;
	sl = setup_list("apple", "pear", "banana", NULL);

	test("stringlist: Remove non-existent");
	assert_stringlist(sl, "pre-remove sl", 3, "apple", "pear", "banana");
	assert_int_ne("removal of 'tomato' from stringlist fails", stringlist_remove(sl, tomato), 0);
	assert_stringlist(sl, "pre-remove sl", 3, "apple", "pear", "banana");

	stringlist_free(sl);
}

void test_stringlist_qsort()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	struct stringlist *sl;
	test("stringlist: Sorting");
	sl = setup_list(b, c, a, NULL);
	assert_stringlist(sl, "pre-sort sl", 3, "bob", "candace", "alice");

	stringlist_sort(sl, STRINGLIST_SORT_ASC);
	assert_stringlist(sl, "post-sort ASC sl", 3, "alice", "bob", "candace");

	stringlist_sort(sl, STRINGLIST_SORT_DESC);
	assert_stringlist(sl, "post-sort DESC sl", 3, "candace", "bob", "alice");

	stringlist_free(sl);

	test("stringlist: Sorting 1-item string list");
	sl = setup_list(b, NULL);
	assert_stringlist(sl, "pre-sort sl", 1, "bob");

	stringlist_sort(sl, STRINGLIST_SORT_ASC);
	assert_stringlist(sl, "post-sort ASC sl", 1, "bob");
	stringlist_sort(sl, STRINGLIST_SORT_DESC);
	assert_stringlist(sl, "post-sort DESC sl", 1, "bob");

	stringlist_free(sl);
}

void test_stringlist_uniq()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	struct stringlist *sl;

	test("stringlist: Uniq");
	sl = setup_list(b, c, a, b, b, c, NULL);
	assert_stringlist(sl, "pre-uniq sl", 6, "bob", "candace", "alice", "bob", "bob", "candace");

	stringlist_uniq(sl);
	assert_stringlist(sl, "post-uniq sl", 3, "alice", "bob", "candace");

	stringlist_free(sl);
}

void test_stringlist_uniq_already()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";

	struct stringlist *sl;

	test("stringlist: Uniq (no changes needed)");
	sl = setup_list(b, c, a, NULL);
	assert_stringlist(sl, "pre-uniq sl", 3, "bob", "candace", "alice");

	stringlist_uniq(sl);
	assert_stringlist(sl, "post-uniq sl", 3, "alice", "bob", "candace");

	stringlist_free(sl);
}

void test_stringlist_diff()
{
	const char *a = "alice";
	const char *b = "bob";
	const char *c = "candace";
	const char *d = "david";
	const char *e = "ethan";

	struct stringlist *sl1, *sl2;

	sl1 = setup_list(b, c, a, NULL);
	sl2 = setup_list(b, c, a, NULL);

	test("stringlist: Diff");
	assert_int_ne("sl1 and sl2 are equivalent", stringlist_diff(sl1, sl2), 0);

	stringlist_add(sl2, d);
	assert_int_eq("after addition of 'david' to sl2, sl1 and sl2 differ", stringlist_diff(sl1, sl2), 0);

	stringlist_add(sl1, e);
	assert_int_eq("after addition of 'ethan' to sl1, sl1 and sl2 differ", stringlist_diff(sl1, sl2), 0);

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_diff_non_uniq()
{
	const char *s1 = "string1";
	const char *s2 = "string2";
	const char *s3 = "string3";

	struct stringlist *sl1, *sl2;

	sl1 = setup_list(s1, s2, s2, NULL);
	sl2 = setup_list(s1, s2, s3, NULL);

	test("stringlist: Diff (non-unique entries)");
	assert_int_eq("sl1 and sl2 are different (forward)", stringlist_diff(sl1, sl2), 0);
	assert_int_eq("sl2 and sl1 are different (reverse)", stringlist_diff(sl2, sl1), 0);

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_diff_single_string()
{
	struct stringlist *sl1, *sl2;
	const char *a = "string a";
	const char *b = "string b";

	sl1 = setup_list(a, NULL);
	sl2 = setup_list(b, NULL);

	test("stringlist: Diff (single string)");
	assert_int_eq("sl1 and sl2 are different (forward)", stringlist_diff(sl1, sl2), 0);
	assert_int_eq("sl2 and sl1 are different (reverse)", stringlist_diff(sl2, sl1), 0);

	stringlist_free(sl1);
	stringlist_free(sl2);
}

void test_stringlist_join()
{
	char *joined = NULL;
	struct stringlist *list = setup_list("item1","item2","item3", NULL);
	struct stringlist *empty = stringlist_new(NULL);

	test("stringlist: Join stringlist with a delimiter");
	free(joined);
	joined = stringlist_join(list, "::");
	assert_str_eq("Check joined string with '::' delimiter", "item1::item2::item3", joined);

	free(joined);
	joined = stringlist_join(list, "");
	assert_str_eq("Check joined string with '' delimiter", "item1item2item3", joined);

	free(joined);
	joined = stringlist_join(list, "\n");
	assert_str_eq("Check joined string with '' delimiter", "item1\nitem2\nitem3", joined);

	free(joined);
	joined = stringlist_join(empty, "!!");
	assert_str_eq("Check empty join", "", joined);

	free(joined);
	stringlist_free(list);
	stringlist_free(empty);
}

void test_stringlist_split()
{
	struct stringlist *list;
	char *joined = "apple--mango--pear";
	char *single = "loganberry";
	char *nulls  = "a space    separated  list";

	test("stringlist: Split strings with delimiters");
	list = stringlist_split(joined, strlen(joined), "--", 0);
	assert_stringlist(list, "split by '--'", 3, "apple", "mango","pear");
	stringlist_free(list);

	list = stringlist_split(joined, strlen(joined), "-", 0);
	assert_stringlist(list, "split by '-'", 5, "apple", "", "mango", "", "pear");
	stringlist_free(list);

	list = stringlist_split(single, strlen(single), "/", 0);
	assert_stringlist(list, "split single-entry list string", 1, "loganberry");
	stringlist_free(list);

	list = stringlist_split(single, strlen(""), "/", 0);
	assert_stringlist(list, "split empty list string", 0);
	stringlist_free(list);

	list = stringlist_split(nulls, strlen(nulls), " ", SPLIT_GREEDY);
	assert_stringlist(list, "greedy split",
		4, "a", "space", "separated", "list");
	stringlist_free(list);
}

void test_stringlist_free_null()
{
	struct stringlist *sl;

	test("stringlist: stringlist_free(NULL)");
	sl = NULL; stringlist_free(sl);
	assert_null("stringist_free(NULL) doesn't segfault", sl);
}

void test_suite_stringlist()
{
	test_stringlist_init();
	test_stringlist_init_with_data();
	test_stringlist_dup();
	test_stringlist_basic_add_remove_search();
	test_stringlist_add_all();
	test_stringlist_add_all_with_expansion();
	test_stringlist_remove_all();
	test_stringlist_expansion();
	test_stringlist_remove_nonexistent();
	test_stringlist_qsort();
	test_stringlist_uniq();
	test_stringlist_uniq_already();

	test_stringlist_diff();
	test_stringlist_diff_non_uniq();
	test_stringlist_diff_single_string();

	test_stringlist_join();
	test_stringlist_split();

	test_stringlist_free_null();
}
