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
#include "../src/mem.h"

TESTS {
	char *s;

	s = NULL; xfree(s);
	pass("xfree doesn't segfault");

	/* test internal implementation of xfree;
	   This is **NOT** the normal use case, but does
	   test an internal safety-net in __xfree. */
	s = NULL; __xfree((void**)s);
	pass("__xfree(&NULL) doesn't segfault");

	ok(s = malloc(42), "allocate 42b buffer");
	xfree(s);
	ok(s == NULL, "xfree(s) nullifies s");

	ok(s = xmalloc(10), "xmalloc allocates memory");
	is_string(s, "", "xmalloc memsets");
	xfree(s);


	ok(xstrdup(NULL) == NULL, "xstrdup(NULL) is NULL");
	s = xstrdup("");
	is_string(s, "", "xstrdup(\"\") is empty string");
	xfree(s);

	s = xstrdup("There once was a man from Nantucket...");
	is_string(s, "There once was a man from Nantucket...",
		"xstrdup can dup strings");
	xfree(s);

	const char *s1, *s2, *s3, *s4, *s5;
	s1 = "This Is A String";
	s2 = s1;
	s3 = "this is a string";
	s4 = "completely off (very different string)";
	s5 = "This Is A String";

	isnt_int(xstrcmp(s1, NULL),   0, "xstrcmp(s1, NULL)");
	isnt_int(xstrcmp(NULL, s1),   0, "xstrcmp(NULL, s1)");
	isnt_int(xstrcmp(NULL, NULL), 0, "xstrcmp(NULL, NULL)");

	ok(xstrcmp(s1, s2) == 0, "xstrcmp, aliased buffer");
	ok(xstrcmp(s1, s5) == 0, "xstrcmp, different buffers");
	ok(xstrcmp(s5, s1) == 0, "xstrcmp, different buffers + flipped");
	ok(xstrcmp(s3, s1)  > 0, "s3 < s1");
	ok(xstrcmp(s4, s3)  < 0, "s4 < s3");
	ok(xstrcmp(s1, s3) != 0, "s1 != s3");


	const char *src = "AAABBBCCCDDDEEEFFF";
	char *ret;
	char dst[19] = "init";

	is_null(xstrncpy(NULL, src,  19), "xstrncpy() with NULL dst");
	is_null(xstrncpy(dst,  NULL, 19), "xstrncpy() with NULL src");
	is_null(xstrncpy(dst,  src,   0), "xstrncpy() with bad lenght");

	isnt_null(ret = xstrncpy(dst, src, 6+1), "xstrncpy() returns pointer");
	ok(dst == ret, "xstrncpy returns dst pointer");
	is_string(dst, "AAABBB", "copy succeeded");

	isnt_null(xstrncpy(dst, src, 19), "xstrncpy(dst, src, 19)");
	is_string(dst, src, "copied whole string");

	isnt_null(xstrncpy(dst, "hi!", 19), "xstrncpy(dst, \"hi!\", 19)");
	is_string(dst, "hi!", "copied whole string");


	char *original[4] = {
		"string1",
		"another string",
		"a third and final string",
		NULL
	};
	char **copy;

	is_null(xarrdup(NULL), "xarrdup(NULL) == NULL");
	copy = NULL; xarrfree(copy);
	pass("xarrfree(NULL) doesn't segfault");
	/* test internal implemenation of xarrfree;
	   This is **NOT** the normal use case, but does
	   test an internal safety-net in __xarrfree. */
	copy = NULL; __xarrfree(NULL);
	is_null(copy, "copy is NULL after __xarrfree");

	copy = xarrdup(original);
	ok(copy != original, "different root pointer");
	is_string(copy[0], original[0], "copy[0] is a faithful copy");
	is_string(copy[1], original[1], "copy[1] is a faithful copy");
	is_string(copy[2], original[2], "copy[2] is a faithful copy");
	is_null(copy[3], "copy[3] is NULL");
	xarrfree(copy);
	is_null(copy, "copy is NULL after xarrfree");


	char buf[129];
	s = string("%s: %u 0x%08x", "Clockwork test build", 1025, 1025);
	isnt_null(s, "string() returns a non-NULL pointer");
	is_string(s, "Clockwork test build: 1025 0x00000401", "string() formats properly");
	free(s);

	memset(buf, 'x', 128); buf[128] = '\0';
	is_int(strlen(buf), 128, "buffer is 128 octets long");
	s = string("%sA%sB%sC%sD", buf, buf, buf, buf);
	is_int(strlen(s), 4+(128*4), "string() dynamically sizes buffer");
	free(s);

	done_testing();
}
