/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

TESTS {
	subtest {
		cw_hash_t *vars;

		const char *tests[][2] = {
			{ "string with no references",
			  "string with no references" },

			{ "string with simple (a-z) ref: $ref1",
			  "string with simple (a-z) ref: this is a reference" },

			{ "multi.level.fact = ${multi.level.fact}",
			  "multi.level.fact = MULTILEVEL" },

			{ "Unknown variable: $unknown",
			  "Unknown variable: " },

			{ "Unknown CREF: ${unknown.cref}",
			  "Unknown CREF: " },

			{ "Escaped ref: \\$ref1",
			  "Escaped ref: $ref1" },

			{ "Multiple refs: $ref1, $ref1 and $name",
			  "Multiple refs: this is a reference, this is a reference and Clockwork" },

			{ "Dollar sign at end of string: \\$",
			  "Dollar sign at end of string: $" },

			{ "Bare dollar sign at end: $",
			  "Bare dollar sign at end: " },

			{ "Bare escape at end: \\",
			  "Bare escape at end: " },

			{ NULL, NULL }
		};

		isnt_null(vars = cw_alloc(sizeof(cw_hash_t)), "created a hash for vars");
		if (!vars) break;

		cw_hash_set(vars, "ref1", "this is a reference");
		cw_hash_set(vars, "name", "Clockwork");
		cw_hash_set(vars, "multi.level.fact", "MULTILEVEL");
		cw_hash_set(vars, "kernel_version", "2.6");

		size_t i;
		for (i = 0; tests[i][0]; i++) {
			char *buf = cw_interpolate(tests[i][0], vars);
			is_string(buf, tests[i][1], "string interpolation");
			free(buf);
		}

		cw_hash_done(vars, 0);
		free(vars);
	}

	subtest {
		cw_strl_t *sl = NULL; cw_strl_free(sl);
		pass("stringist_free(NULL) doesn't segfault");
	}

	subtest {
		cw_strl_t *sl;
		isnt_null(sl = cw_strl_new(NULL), "stringlist created");
		is_int(sl->num, 0, "stringlist is empty");
		isnt_int(sl->len, 0, "stringlist has capacity");
		cw_strl_free(sl);
	}

	subtest {
		cw_strl_t *sl;
		char *data[33];
		char buf[32];
		int i;

		for (i = 0; i < 32; i++) {
			snprintf(buf, 32, "data%i", i);
			data[i] = strdup(buf);
		}
		data[32] = NULL;

		isnt_null(sl = cw_strl_new(data), "stringlist created");
		is_int(sl->num, 32, "list has 32 strings");
		ok(sl->len > 32, "list has more than 32 slots");

		is_string(sl->strings[0], "data0", "strings[0]");
		is_string(sl->strings[14], "data14", "strings[14]");
		is_string(sl->strings[28], "data28", "strings[28]");
		is_string(sl->strings[31], "data31", "strings[31]");
		is_null(sl->strings[32], "string[32] is null");

		cw_strl_free(sl);
		for (i = 0; i < 32; i++) {
			free(data[i]);
		}
	}

	subtest {
		// test auto-expansion
		cw_strl_t *sl;
		size_t max, i;
		char buf[32];
		char assertion[128];

		isnt_null(sl = cw_strl_new(NULL), "created a stringlist");

		max = sl->len;
		for (i = 0; i < max * 2 + 1; i++) {
			snprintf(buf, 32, "string%u", (unsigned int)i);
			snprintf(assertion, 128, "add 'string%u' to stringlist", (unsigned int)i);
			ok(cw_strl_add(sl, buf) == 0, assertion);
			is_int(sl->num, i + 1, "total number of strings");
		}

		ok(sl->len != max, "stringlist length changed");

		cw_strl_free(sl);
	}

	subtest {
		cw_strl_t *orig, *dup;
		char *data[5];
		char buf[32];
		int i;

		for (i = 0; i < 4; i++) {
			snprintf(buf, 32, "data%u", i);
			data[i] = strdup(buf);
		}
		data[4] = NULL;

		orig = cw_strl_new(data);
		dup  = cw_strl_dup(orig);

		is_int(orig->num, 4, "orignal list has 4 items");
		is_int(dup->num,  4, "duplicate list has 4 items");

		is_string(orig->strings[0], "data0", "orig[0]");
		is_string(dup->strings[0],  "data0", "dup[0]");

		is_string(orig->strings[1], "data1", "orig[1]");
		is_string(dup->strings[1],  "data1", "dup[1]");

		is_string(orig->strings[2], "data2", "orig[2]");
		is_string(dup->strings[2],  "data2", "dup[2]");

		is_string(orig->strings[3], "data3", "orig[3]");
		is_string(dup->strings[3],  "data3", "dup[3]");

		cw_strl_free(orig);
		cw_strl_free(dup);

		for (i = 0; i < 4; i++) {
			free(data[i]);
		}
	}

	subtest {
		cw_strl_t *sl;

		isnt_null(sl = cw_strl_new(NULL), "got a stringlist");
		ok(cw_strl_add(sl, "first string")  == 0, "added 'first string'");
		ok(cw_strl_add(sl, "second string") == 0, "added 'second string'");
		is_int(sl->num, 2, "list has 2 strings");
		is_string(sl->strings[0], "first string", "strings[0]");
		is_string(sl->strings[1], "second string", "strings[1]");
		is_null(sl->strings[2], "strings[2] is NULL");

		ok(cw_strl_search(sl, "first string")   == 0, "found 'first string' in list");
		ok(cw_strl_search(sl, "second string")  == 0, "found 'second string' in list");
		ok(cw_strl_search(sl, "no such string") != 0, "did not find 'no such string' in list");

		ok(cw_strl_remove(sl, "first string") == 0, "removed 'first string'");
		is_int(sl->num, 1, "list only has 1 string now");
		is_string(sl->strings[0], "second string", "strings[0]");
		is_null(sl->strings[1], "strings[1] is NULL");

		ok(cw_strl_search(sl, "first string")   != 0, "did not find 'first string' in list");
		ok(cw_strl_search(sl, "second string")  == 0, "found 'second string' in list");
		ok(cw_strl_search(sl, "no such string") != 0, "did not find 'no such string' in list");

		cw_strl_free(sl);
	}

	subtest {
		cw_strl_t *list;
		char *joined = "apple--mango--pear";
		char *single = "loganberry";
		char *nulls  = "a space    separated  list";

		list = cw_strl_split(joined, strlen(joined), "--", 0);
		is_int(list->num, 3, "list has 3 strings");
		is_string(list->strings[0], "apple", "strings[0]");
		is_string(list->strings[1], "mango", "strings[1]");
		is_string(list->strings[2], "pear",  "strings[2]");
		is_null(list->strings[3], "strings[3] is NULL");
		cw_strl_free(list);

		list = cw_strl_split(joined, strlen(joined), "-", 0);
		is_int(list->num, 5, "list has 5 strings");
		is_string(list->strings[0], "apple", "strings[0]");
		is_string(list->strings[1], "",      "strings[1]");
		is_string(list->strings[2], "mango", "strings[2]");
		is_string(list->strings[3], "",      "strings[3]");
		is_string(list->strings[4], "pear",  "strings[4]");
		is_null(list->strings[5], "strings[5] is NULL");
		cw_strl_free(list);

		list = cw_strl_split(single, strlen(single), "/", 0);
		is_int(list->num, 1, "list has 1 string");
		is_string(list->strings[0], "loganberry", "strings[0]");
		is_null(list->strings[1], "strings[1] is NULL");
		cw_strl_free(list);

		list = cw_strl_split(single, strlen(""), "/", 0);
		is_int(list->num, 0, "list is empty");
		is_null(list->strings[0], "strings[0] is NULL");
		cw_strl_free(list);

		list = cw_strl_split(nulls, strlen(nulls), " ", SPLIT_GREEDY);
		is_int(list->num, 4, "list has 4 strings");
		is_string(list->strings[0], "a",         "strings[0]");
		is_string(list->strings[1], "space",     "strings[1]");
		is_string(list->strings[2], "separated", "strings[2]");
		is_string(list->strings[3], "list",      "strings[3]");
		is_null(list->strings[4], "strings[4] is NULL");
		cw_strl_free(list);
	}

	subtest {
		char *joined = NULL;
		cw_strl_t *list  = cw_strl_new(NULL);
		cw_strl_t *empty = cw_strl_new(NULL);

		cw_strl_add(list, "item1");
		cw_strl_add(list, "item2");
		cw_strl_add(list, "item3");

		is_string(
			joined = cw_strl_join(list, "::"),
			"item1::item2::item3",
			"Join with '::'");
		free(joined);

		is_string(
			joined = cw_strl_join(list, ""),
			"item1item2item3",
			"Join with ''");
		free(joined);

		is_string(
			joined = cw_strl_join(list, "\n"),
			"item1\nitem2\nitem3",
			"Join with '\n'");
		free(joined);

		is_string(
			joined = cw_strl_join(empty, "!!"),
			"", "empty list joined by arbitrary delimiter");
		free(joined);

		cw_strl_free(list);
		cw_strl_free(empty);
	}

	/* NOTE: from here on, we can use stringlist join / split */

	subtest {
		cw_strl_t *sl;
		const char *list = "apple pear banana";
		char *tmp;

		sl = cw_strl_split(list, strlen(list), " ", 0);
		ok(cw_strl_remove(sl, "tomato") != 0,
			"failed to remove non-existent string from list");
		is_string(tmp = cw_strl_join(sl, " "), list,
			"failed removal leaves string list intact");
		free(tmp);

		cw_strl_free(sl);
	}

	subtest {
		cw_strl_t *sl1, *sl2;
		const char *list1 = "lorem ipsum dolor";
		const char *list2 = "sit amet consectetur";
		char *tmp;

		sl1 = cw_strl_split(list1, strlen(list1), " ", 0);
		sl2 = cw_strl_split(list2, strlen(list2), " ", 0);

		ok(cw_strl_add_all(sl1, sl2) == 0, "combined sl1 and sl2");
		is_string(
			tmp = cw_strl_join(sl1, " "),
			"lorem ipsum dolor sit amet consectetur",
			"combined list (destination)");
		free(tmp);

		is_string(
			tmp = cw_strl_join(sl2, " "),
			"sit amet consectetur",
			"added list (sl2) is untouched");
		free(tmp);

		cw_strl_free(sl1);
		cw_strl_free(sl2);
	}

	subtest {
		cw_strl_t *sl1, *sl2;
		int capacity = 0;

		const char *list1 = "a b c d e f g h i j k l m n";
		const char *list2 = "o p q";
		char *tmp;

		sl1 = cw_strl_split(list1, strlen(list1), " ", 0);
		sl2 = cw_strl_split(list2, strlen(list2), " ", 0);

		capacity = sl1->len;
		ok(sl1->num + sl2->num > sl1->len, "combined list len > sl1 capacity");
		ok(cw_strl_add_all(sl1, sl2) == 0, "combined lists");

		is_string(
			tmp = cw_strl_join(sl1, " "),
			"a b c d e f g h i j k l m n o p q",
			"combined lists properly");
		free(tmp);
		ok(sl1->len > capacity, "new capacity > old capacity");

		cw_strl_free(sl1);
		cw_strl_free(sl2);
	}

	subtest {
		cw_strl_t *sl1, *sl2;
		const char *list1 = "lorem ipsum dolor";
		const char *list2 = "ipsum dolor sit";
		char *tmp;

		sl1 = cw_strl_split(list1, strlen(list1), " ", 0);
		sl2 = cw_strl_split(list2, strlen(list2), " ", 0);

		ok(cw_strl_remove_all(sl1, sl2) == 0, "removed sl2 from sl1");
		is_string(
			tmp = cw_strl_join(sl1, " "),
			"lorem", "removal verified");
		free(tmp);
		is_string(
			tmp = cw_strl_join(sl2, " "),
			"ipsum dolor sit",
			"sl2 (list of things to remove) is untouched");
		free(tmp);

		cw_strl_free(sl1);
		cw_strl_free(sl2);
	}

	subtest {
		cw_strl_t *sl;
		const char *unsorted = "bob candace alice";
		char *tmp;

		sl = cw_strl_split(unsorted, strlen(unsorted), " ", 0);
		cw_strl_sort(sl, STRL_ASC);
		is_string(
			tmp = cw_strl_join(sl, " "),
			"alice bob candace",
			"sorted asc alpha");
		free(tmp);
		cw_strl_free(sl);

		sl = cw_strl_split(unsorted, strlen(unsorted), " ", 0);
		cw_strl_sort(sl, STRL_DESC);
		is_string(
			tmp = cw_strl_join(sl, " "),
			"candace bob alice",
			"sorted desc alpha");
		free(tmp);
		cw_strl_free(sl);
	}

	subtest {
		cw_strl_t *sl;
		char *tmp;

		sl = cw_strl_new(NULL);
		cw_strl_add(sl, "bob");

		cw_strl_sort(sl, STRL_ASC);
		is_string(
			tmp = cw_strl_join(sl, " "),
			"bob", "sorted 1 item asc alpha");
		free(tmp);

		cw_strl_sort(sl, STRL_DESC);
		is_string(
			tmp = cw_strl_join(sl, " "),
			"bob", "sorted 1 item asc alpha");
		free(tmp);

		cw_strl_free(sl);
	}

	subtest {
		cw_strl_t *sl;
		const char *dupes = "bob candace alice bob bob candace";
		const char *uniq  = "bob candace alice";
		char *tmp;

		sl = cw_strl_split(dupes, strlen(dupes), " ", 0);
		cw_strl_uniq(sl);
		is_string(
			tmp = cw_strl_join(sl, " "),
			"alice bob candace",
			"cw_strl_uniq removed all duplicates (and sorted!)");
		free(tmp);

		cw_strl_free(sl);
		sl = cw_strl_split(uniq, strlen(uniq), " ", 0);
		cw_strl_uniq(sl);
		is_string(
			tmp = cw_strl_join(sl, " "),
			"alice bob candace",
			"cw_strl_uniq removed all duplicates (and sorted!)");
		free(tmp);
		cw_strl_free(sl);
	}

	subtest {
		const char *list = "alice bob candace";
		cw_strl_t *sl1, *sl2;

		sl1 = cw_strl_split(list, strlen(list), " ", 0);
		sl2 = cw_strl_split(list, strlen(list), " ", 0);
		ok(cw_strl_diff(sl1, sl2) != 0, "sl1 == sl2");

		ok(cw_strl_add(sl2, "david") == 0, "added 'david' to sl2");
		ok(cw_strl_diff(sl1, sl2) == 0, "sl1 != sl2 (+david)");

		ok(cw_strl_add(sl1, "david") == 0, "added 'david' to sl1");
		ok(cw_strl_diff(sl1, sl2) != 0, "sl1 == sl2");

		cw_strl_free(sl1);
		cw_strl_free(sl2);
	}

	subtest {
		const char *list1 = "alice bob candy";
		const char *list2 = "alice bob bob";
		cw_strl_t *sl1, *sl2;

		sl1 = cw_strl_split(list1, strlen(list1), " ", 0);
		sl2 = cw_strl_split(list2, strlen(list2), " ", 0);

		ok(cw_strl_diff(sl1, sl2) == 0, "sl1 != sl2 (forward)");
		ok(cw_strl_diff(sl2, sl1) == 0, "sl2 != sl1 (reverse)");

		cw_strl_free(sl1);
		cw_strl_free(sl2);
	}

	subtest {
		const char *list1 = "apples";
		const char *list2 = "oranges";
		cw_strl_t *sl1, *sl2;

		sl1 = cw_strl_split(list1, strlen(list1), " ", 0);
		sl2 = cw_strl_split(list2, strlen(list2), " ", 0);

		ok(cw_strl_diff(sl1, sl2) == 0, "sl1 != sl2 (forward)");
		ok(cw_strl_diff(sl2, sl1) == 0, "sl2 != sl1 (reverse)");

		cw_strl_free(sl1);
		cw_strl_free(sl2);
	}

	subtest {
		cw_strl_t *X, *Y;
		cw_strl_t *intersect;
		char *tmp;

		X = cw_strl_split("l m n",   5, " ", 0);
		Y = cw_strl_split("m n o p", 7, " ", 0);
		intersect = cw_strl_intersect(X, Y);

		is_string(
			tmp = cw_strl_join(intersect, " "),
			"m n", "X intersect Y");
		free(tmp);

		cw_strl_free(intersect);
		cw_strl_free(X);
		cw_strl_free(Y);
	}

	subtest {
		char *s;

		isnt_null(s = cw_string("%s: %u 0x%08x", "Clockwork test build", 1025, 1025), "cw_string() succeeds");
		is_string(s, "Clockwork test build: 1025 0x00000401", "formats properly");
		free(s);
	}

	subtest {
		char *s;
		char buf[129];

		// large buffer required
		memset(buf, 'x', 128); buf[128] = '\0';
		s = cw_string("%sA%sB%sC%sD", buf, buf, buf, buf);
		is_int(strlen(s), 4+(128*4), "s should be 4+(128x4) octets long");
		free(s);
	}

	done_testing();
}
