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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "stringlist.h"
#include "mem.h"

#define INIT_LEN   16

#define EXPAND_FACTOR 8
#define EXPAND_LEN(x) (x / EXPAND_FACTOR + 1) * EXPAND_FACTOR

static int _stringlist_expand(stringlist*, size_t);
static int _stringlist_reduce(stringlist*);
static size_t _stringlist_capacity(stringlist*);

static int _stringlist_expand(stringlist *sl, size_t expand)
{
	assert(sl);         // LCOV_EXCL_LINE
	assert(expand > 0); // LCOV_EXCL_LINE

	char **s;
	expand = EXPAND_LEN(expand) + sl->len;
	s = realloc(sl->strings, expand * sizeof(char *));
	if (!s) {
		return -1;
	}

	sl->strings = s;
	for (; sl->len < expand; sl->len++) {
		sl->strings[sl->len] = NULL;
	}

	return 0;
}

/* Remove NULL strings from $sl. */
static int _stringlist_reduce(stringlist *sl)
{
	char **ins; char **ptr, **end;

	ptr = ins = sl->strings;
	end = sl->strings + sl->num;
	while (ins < end) {
		while (!*ptr && ptr++ < end) {
			sl->num--;
		}

		/*
		if (ptr == end) {
			break;
		}
		*/

		*ins++ = *ptr++;
	}

	return 0;
}

static size_t _stringlist_capacity(stringlist* sl)
{
	return sl->len - 1 - sl->num;
}

/*****************************************************************/

int STRINGLIST_SORT_ASC(const void *a, const void *b)
{
	/* params are pointers to char* */
	return strcmp(* (char * const *) a, * (char * const *) b);
}

int STRINGLIST_SORT_DESC(const void *a, const void *b)
{
	/* params are pointers to char* */
	return -1 * strcmp(* (char * const *) a, * (char * const *) b);
}

/**
  Create a new String List.

  If $src is not NULL, the new list will contain copies of all
  strings in $src, which must be a NULL-terminated array of
  NULL-terminated character strings.

  If $src is NULL, the new list will be empty.

  On success, a new string list is returned.  This pointer must
  be freed via @stringlist_free.

  On failure, any memory allocated by `stringlist_new` will be
  freed and the NULL will be returned.
 */
stringlist* stringlist_new(char **src)
{
	stringlist *sl;
	char **t;

	sl = xmalloc(sizeof(stringlist));
	if (src) {
		t = src;
		while (*t++)
			;
		sl->num = t - src - 1;
		sl->len = EXPAND_LEN(sl->num);
	} else {
		sl->num = 0;
		sl->len = INIT_LEN;
	}

	sl->strings = xmalloc(sl->len * sizeof(char *));

	if (src) {
		for (t = sl->strings; *src; src++, t++) {
			*t = strdup(*src);
		}
	}

	return sl;
}

/**
  Duplicate $orig.

  In the following example, $new1 and $new2 will contain
  equivalent strings, in different parts of memory.

  <code>
  stringlist *orig = generate_a_list_of_strings()

  // this statement...
  stringlist *new1 = stringlist_dup(orig);

  // ...is equivalent to this one
  stringlist *new2 = stringlist_new(orig->strings);
  </code>

  On success, a new stringlist that is equivalent to $orig
  is returned.  On failure, NULL is returned.
 */
stringlist* stringlist_dup(stringlist *orig)
{
	return stringlist_new(orig->strings);
}

/**
  Free the $sl string list.
 */
void stringlist_free(stringlist *sl)
{
	size_t i;
	if (sl) {
		for_each_string(sl,i) {
			free(sl->strings[i]);
		}
		free(sl->strings);
	}
	free(sl);
}

/**
  Sort $sl, using $cmp to compare strings.

  Comparison functions operate according to existing convention;  For each
  pair of adjacent strings, the function will be called and passed both
  strings.  It should return an integer less than, or greater than zero if
  the first string should appear before or after the second.  If both
  strings are equal, the comparator should return 0, and the sort order
  will be undefined.

  Two basic comparators are defined already: `STRINGLIST_SORT_ASC` and
  `STRINGLIST_SORT_DESC` for sorting alphabetically and reverse alphabetically,
  respectively.

  **Note:** Sorting is done in-place; $sl *will* be modified.
 */
void stringlist_sort(stringlist* sl, sl_comparator cmp)
{
	assert(sl);  // LCOV_EXCL_LINE
	assert(cmp); // LCOV_EXCL_LINE

	if (sl->num < 2) { return; }
	qsort(sl->strings, sl->num, sizeof(char *), cmp);
}

/**
  Remove duplicate strings from $sl.

  This function will reduce $sl such that it only contains unique values.
  In order to do that, $sl will first be sorted (using @stringlist_sort
  and `STRINGLIST_SORT_ASC`), so that duplicate values are adjacent.

  Example:

  <code>
  stringlist *fruit = stringlist_new(NULL);

  stringlist_add(fruit, "pear");
  stringlist_add(fruit, "banana");
  stringlist_add(fruit, "pear");
  stringlist_add(fruit, "apple");
  stringlist_add(fruit, "pear");

  // at this point, the list is 'pear', 'banana', 'pear', 'apple', 'pear'

  stringlist_uniq(fruit);

  // now, the list is 'apple', 'banana', 'pear'
  </code>

  **Note:** De-duplication is done in-place; $sl *will* be modified.
 */
void stringlist_uniq(stringlist *sl)
{
	assert(sl); // LCOV_EXCL_LINE

	size_t i;

	if (sl->num < 2) { return; }

	stringlist_sort(sl, STRINGLIST_SORT_ASC);
	for (i = 0; i < sl->num - 1; i++) {
		if (strcmp(sl->strings[i], sl->strings[i+1]) == 0) {
			free(sl->strings[i]);
			sl->strings[i] = NULL;
		}
	}
	_stringlist_reduce(sl);
}

/**
  Look for $needle in $sl.

  If $needle is found in $list, returns 0.
  Otherwise, returns non-zero.
 */
int stringlist_search(const stringlist *sl, const char* needle)
{
	assert(sl);     // LCOV_EXCL_LINE
	assert(needle); // LCOV_EXCL_LINE

	size_t i;
	for_each_string(sl,i) {
		if ( strcmp(sl->strings[i], needle) == 0 ) {
			return 0;
		}
	}
	return -1;
}

/**
  Append a copy of $str to $sl.

  On success, returns 0.  On failure, returns non-zero.
 */
int stringlist_add(stringlist *sl, const char* str)
{
	assert(sl);  // LCOV_EXCL_LINE
	assert(str); // LCOV_EXCL_LINE

	/* expand as needed */
	if (_stringlist_capacity(sl) == 0 && _stringlist_expand(sl, 1) != 0) {
		return -1;
	}

	sl->strings[sl->num++] = strdup(str);
	sl->strings[sl->num] = NULL;

	return 0;
}

/**
  Append all strings from $src to $dst.

  The order of arguments can be remembered by envsioning the call
  as a replacement for a simpler one: `$dest += $src`

  On success, returns 0.  On failure, returns non-zero and $dest is
  unmodified.
 */
int stringlist_add_all(stringlist *dst, const stringlist *src)
{
	assert(src); // LCOV_EXCL_LINE
	assert(dst); // LCOV_EXCL_LINE

	size_t i;

	if (_stringlist_capacity(dst) < src->num && _stringlist_expand(dst, src->num)) {
		return -1;
	}

	for_each_string(src,i) {
		dst->strings[dst->num++] = strdup(src->strings[i]);
	}
	dst->strings[dst->num] = NULL;

	return 0;
}

/**
  Remove the first occurrence of $str from $sl.

  If $value appears in $sl multiple times, multple calls
  are needed to remove them.  This is best done in a while loop:

  <code>
  while (stringlist_remove(sl, "a string") == 0)
      ;
  </code>

  If $str was found in, and removed from $sl, returns 0.
  Otherwise, returns non-zero.
 */
int stringlist_remove(stringlist *sl, const char *str)
{
	assert(sl);  // LCOV_EXCL_LINE
	assert(str); // LCOV_EXCL_LINE

	char *removed = NULL;
	size_t i;
	for (i = 0; i < sl->num; i++) {
		if (strcmp(sl->strings[i], str) == 0) {
			removed = sl->strings[i];
			break;
		}
	}

	for (; i < sl->num; i++) {
		sl->strings[i] = sl->strings[i+1];
	}

	if (removed) {
		sl->num--;
		free(removed);
		return 0;
	}

	return -1;
}

/**
  Remove strings in $src from $est.

  The order of the arguments can be remembered by thinking of this
  call as in terms of a simpler one: `$dst -= $src`

  **Note:** this function deals with duplicate values naively,
  only removing the first such occurrence in $dst for each
  occurrence in $src.

  <code>
  stringlist *list = stringlist_new(NULL);
  stringlist *rm = stringlist_new(NULL);

  stringlist_add(list, "a");
  stringlist_add(list, "b");
  stringlist_add(list, "a");
  stringlist_add(list, "a");

  stringlist_add(rm, "a");
  stringlist_add(rm, "c");
  stringlist_add(rm, "a");

  // list = [ a, b, a, a ]
  // rm   = [ a, c, a ]

  stringlist_remove_all(list, rm);
  // list - rm = [ a, b ]
  </code>

  On success, returns 0.  On failure, returns non-zero.
 */
int stringlist_remove_all(stringlist *dst, stringlist *src)
{
	assert(src); // LCOV_EXCL_LINE
	assert(dst); // LCOV_EXCL_LINE

	size_t s, d;
	for_each_string(dst,d) {
		for_each_string(src,s) {
			if ( strcmp(dst->strings[d], src->strings[s]) == 0 ) {
				free(dst->strings[d]);
				dst->strings[d] = NULL;
				break;
			}
		}
	}

	return _stringlist_reduce(dst);
}

/**
  Get the intersection of $a and $b.

  As in set theory, the intersection of two string lists is another
  string list containing the strings common to both $a and $b.

  More rigorously, if X is the set [ l, m, n ] and Y is the set
  [ m, n, o, p ], then the intersection of X and Y is the set [ m, n ].

  On success, returns a new string list containing strings common to
  $a and $b.  On falure, returns NULL.
 */
stringlist *stringlist_intersect(const stringlist *a, const stringlist *b)
{
	stringlist *intersect = stringlist_new(NULL);
	size_t i, j;

	for_each_string(a,i) {
		for_each_string(b,j) {
			if (strcmp(a->strings[i], b->strings[j]) == 0) {
				stringlist_add(intersect, a->strings[i]);
			}
		}
	}

	return intersect;
}

/**
  Compare $a and $b for equivalency.

  If $a and $b contain the same strings, in the same order,
  returns 0.  Otherwise, the two lists are not equivalent, and
  non-zero if returned.
 */
int stringlist_diff(stringlist* a, stringlist* b)
{
	assert(a); // LCOV_EXCL_LINE
	assert(b); // LCOV_EXCL_LINE

	size_t i;

	if (a->num != b->num) { return 0; }
	for_each_string(a,i) {
		if (stringlist_search(b, a->strings[i]) != 0
		 || stringlist_search(a, b->strings[i]) != 0) {
			return 0;
		}
	}

	return -1; /* equivalent */
}

/**
  Join strings in $list, separated by $delim, into a single string.

  This function works like Perl's `join` function and Ruby's
  `String#join` method.

  Examples:
  <code>
  stringlist *l = stringlist_new(NULL);
  stringlist_add(l, "red");
  stringlist_add(l, "green");
  stringlist_add(l, "blue");

  // A few different ways to join stringlists:
  stringlist_join(l, ", ");    // "red, green, blue"
  stringlist_join(l, " and "); // "red and green and blue"
  stringlist_join(l, ":");     // "red:green:blue"
  </code>

  For the special case of an empty list, `stringlist_join` will return
  an empty string ("") no matter what $delim is.

  Returns a dynamically-allocated string containing each string in
  $list, separated by $delim, or NULL on failure.  The returned pointer
  must be freed by the caller, using `free(3)`.
 */
char* stringlist_join(stringlist *list, const char *delim)
{
	assert(list);  // LCOV_EXCL_LINE
	assert(delim); // LCOV_EXCL_LINE

	size_t i, len = 0, delim_len = strlen(delim);
	char *joined, *ptr;

	if (list->num == 0) {
		return strdup("");
	}

	for_each_string(list,i) {
		len += strlen(list->strings[i]);
	}
	len += (list->num - 1) * delim_len;

	ptr = joined = xmalloc(len + 1);
	for_each_string(list,i) {
		if (i != 0) {
			memcpy(ptr, delim, delim_len);
			ptr += delim_len;
		}

		len = strlen(list->strings[i]);
		memcpy(ptr, list->strings[i], len);
		ptr += len;
	}
	*ptr = '\0';

	return joined;
}

/**
  Split $str on $delim and return the result.

  This function works like Perl's `split` function and Ruby's
  `String#split` method.

  $len contains the length of $str in bytes, usually calculated
  via `strlen(3)`.

  $opt controls how the split algorithm deals with empty tokens.

  - **SPLIT_NORMAL** - Empty tokens are ignored
  - **SPLIT_GREEDY** - Empty tokens are not ignored

  Examples:

  <code>
  stringlist *l = stringlist_split("one::two::three", 15, "::", 0);
  // List l now contains three strings: 'one', 'two', and 'three'
  </code>

  On success, returns a new stringlist.  On failure, returns NULL.
 */
stringlist* stringlist_split(const char *str, size_t len, const char *delim, int opt)
{
	stringlist *list = stringlist_new(NULL);
	const char *a, *b, *end = str + len;
	size_t delim_len = strlen(delim);
	char *item;

	a = str;
	while (a < end) {
		for (b = a; b < end; b++) {
			if (strncmp(b, delim, delim_len) == 0) {
				break;
			}
		}

		if (opt != SPLIT_GREEDY || a != b) {
			item = xmalloc(b - a + 1);
			memcpy(item, a, b - a);
			item[b-a] = '\0';

			if (stringlist_add(list, item) != 0) {
				stringlist_free(list);
				return NULL;
			}

			free(item);
		}
		a = b + delim_len;
	}

	return list;
}

