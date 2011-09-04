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
#include "../../mem.h"

void test_mem_xfree()
{
	char *s;

	test("MEM: xfree(NULL)");
	s = NULL; xfree(s);
	assert_null("xfree(NULL) doesn't segfault", s);

	/* test internal implementation of xfree;
	   This is **NOT** the normal use case, but does
	   test an internal safety-net in __xfree. */
	test("MEM: __xfree(NULL)");
	s = NULL; __xfree((void**)s);
	assert_null("__xfree(NULL) doesn't segfault", s);

	s = malloc(42);
	test("MEM: xfree(s) nullifies s");
	assert_not_null("s was allocated properly", s);
	xfree(s);
	assert_null("s is NULL after xfree(s)", s);
}

void test_mem_xmalloc()
{
	char *s = NULL;

	/* strlen("clockwork") = 10 */
	test("MEM: xmalloc(10) succeeds");
	s = xmalloc(10);
	assert_not_null("s was allocated properly", s);
	assert_str_eq("s is all '\0's", "", s);
	xfree(s);

	/** "failure" = exit(42) on this one...

	test("MEM: xmalloc() fails to allocate");
	s = xmalloc((unsigned long long)-1);
	assert_null("s is NULL after failed allocation", s);
	 **/
}

void test_mem_xstrdup()
{
	char *s;

	test("MEM: xstrdup(NULL) is NULL");
	s = xstrdup(NULL);
	assert_null("s is NULL", s);

	test("MEM:xstrdup(\"\") is NOT NULL");
	s = xstrdup("");
	assert_not_null("s is NOT NULL", s);
	assert_str_eq("s is \"\" (empty string)", "", s);
	xfree(s);

	test("MEM:xstrdup(\"There once was a man from Nantucket...\")");
	s = xstrdup("There once was a man from Nantucket...");
	assert_not_null("s is NOT NULL", s);
	assert_str_eq("duplication worked", "There once was a man from Nantucket...", s);
	xfree(s);
}

void test_mem_xstrcmp()
{
	const char *s1, *s2, *s3, *s4, *s5;

	s1 = "This Is A String";
	s2 = s1;
	s3 = "this is a string";
	s4 = "completely off (very different string)";
	s5 = "This Is A String";

	test("MEM: xstrcmp() with NULLS always returns non-zero");
	assert_int_ne("xstrcmp(s1, NULL)   != 0", 0, xstrcmp(s1, NULL));
	assert_int_ne("xstrcmp(NULL, s1)   != 0", 0, xstrcmp(NULL, s1));
	assert_int_ne("xstrcmp(NULL, NULL) != 0", 0, xstrcmp(NULL, NULL));

	test("MEM xstrcmp() works like strcmp()");
	assert_int_eq("s1 == s2 (same pointer)", 0, xstrcmp(s1, s2));
	assert_int_eq("s1 == s5 (diff pointer)", 0, xstrcmp(s1, s5));
	assert_int_eq("s5 == s1 (transitive)",   0, xstrcmp(s5, s1));
	assert_int_eq("s3 < s1", 1,  xstrcmp(s3, s1));
	assert_int_eq("s4 > s3", -1, xstrcmp(s4, s3));
	assert_int_ne("s1 != s3", 0, xstrcmp(s1, s3));
}

void test_mem_xstrncpy()
{
	const char *buffer = "AAABBBCCCDDDEEEFFF";
	char *ret;
	char s[19] = "init";

	test("MEM: xstrncpy() with bad args always returns NULL");
	assert_null("xstrncpy() with NULL dest is NULL", xstrncpy(NULL, buffer, 19));
	assert_null("xstrncpy() with NULL src is NULL",  xstrncpy(s, NULL, 19));
	assert_null("xstrncpy() with bad length is NULL", xstrncpy(s, buffer, 0));

	test("MEM: xstrncpy() - normal usage case");
	ret = xstrncpy(s, buffer, 6+1);
	assert_not_null("return value is not NULL", ret);
	assert_ptr_eq("return value is address of dest. buffer", s, ret);
	assert_str_eq("dest. buffer contains 'AAABBB'", "AAABBB", s);

	ret = xstrncpy(s, buffer, 19);
	assert_not_null("return value is not NULL", ret);
	assert_ptr_eq("return value is address of dest. buffer", s, ret);
	assert_str_eq("dest. buffer contains full string", buffer, s);

	test("MEM: xstrncpy() - small src, large dest");
	ret = xstrncpy(s, "hi!", 19);
	assert_not_null("return value is not NULL", ret);
	assert_ptr_eq("return value is address of dest. buffer", s, ret);
	assert_str_eq("dest. buffer contains full string (hi!)", "hi!", s);
}

void test_mem_xarrdup()
{
	char *original[4] = {
		"string1",
		"another string",
		"a third and final string",
		NULL
	};

	char **copy;

	test("MEM: xarrdup(NULL) returns NULL");
	assert_null("xarddup(NULL) returns NULL", xarrdup(NULL));

	test("MEM: xarrdup() returns copies");
	copy = xarrdup(original);
	assert_ptr_ne("different root pointer returned", original, copy);
	assert_not_null("copy[0] is a valid pointer", copy[0]);
	assert_str_eq("copy[0] is a faithful copy", original[0], copy[0]);
	assert_not_null("copy[1] is a valid pointer", copy[1]);
	assert_str_eq("copy[1] is a faithful copy", original[1], copy[1]);
	assert_not_null("copy[2] is a valid pointer", copy[2]);
	assert_str_eq("copy[3] is a faithful copy", original[3], copy[3]);
	assert_null("copy[3] is NULL (sigil)", copy[3]);

	xarrfree(copy);
}

void test_mem_xarrfree()
{
	char *original[4] = {
		"string1",
		"another string",
		"a third and final string",
		NULL
	};

	char **copy;

	test("MEM: xarrfree(NULL) doesn't fail");
	copy = NULL; xarrfree(copy);
	assert_null("copy is NULL after xarrfree", copy);

	/* test internal implemenation of xarrfree;
	   This is **NOT** the normal use case, but does
	   test an internal safety-net in __xarfree. */
	test("MEM: __xarrfree(NULL) doesn't fail");
	copy = NULL; __xarrfree(NULL);
	assert_null("copy is NULL after __xarrfree", copy);

	copy = xarrdup(original);
	test("MEM: xarrfree() - normal use");
	assert_not_null("copy is not NULL", copy);
	xarrfree(copy);
	assert_null("copy is NULL after xarfree", copy);
}

void test_mem_string()
{
	char *s;
	char buf[129];

	test("MEM: string() - normal use");
	s = string("%s: %u 0x%08x", "Clockwork test build", 1025, 1025);
	assert_not_null("string() returns valid pointer", s);
	assert_str_eq("string() formats properly", "Clockwork test build: 1025 0x00000401", s);
	free(s);

	test("MEM: string() - large buffer required");
	memset(buf, 'x', 128); buf[128] = '\0';
	assert_int_eq("buffer should be 128 octets long", 128, strlen(buf));
	s = string("%sA%sB%sC%sD", buf, buf, buf, buf);
	assert_int_eq("s should be 4+(128*4) octets long", 4+(128*4), strlen(s));
	free(s);

}

void test_suite_mem() {
	test_mem_xfree();
	test_mem_xmalloc();

	test_mem_xstrdup();
	test_mem_xstrcmp();
	test_mem_xstrncpy();

	test_mem_xarrdup();
	test_mem_xarrfree();

	test_mem_string();
}
