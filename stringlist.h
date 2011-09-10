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

#ifndef STRINGLIST_H
#define STRINGLIST_H

#include <sys/types.h>

/**
  A String List

  A stringlist is a specialized data structure designed for
  efficient storage and management of a list of strings.  It
  wraps around the C representation as a NULL-terminated array
  of character pointers, and extends the idiom with memory
  management methods and "standard" string list manipulation
  routines.

  ### Basic Usage ############################################

  Stringlist allocation and de-allocation is performed through
  the @stringlist_new and @stringlist_free functions.  These two
  functions allocate memory from the heap, and then free that
  memory.

  <code>
  // allocate a new, empty stringlist
  stringlist *lst = stringlist_new(NULL);

  // do something interesting with the list...
  stringlist_add(fruit, "pear");
  stringlist_add(fruit, "apple");
  stringlist_add(fruit, "cantaloupe");

  // free memory allocated by stringlist_new and stringlist_add
  stringlist_free(lst);
  </code>

  The first argument to @stringlist_new is a C-style `char**`
  string array, and can be used to "seed" the stringlist with
  starting values.  For example, to deal with command-line
  arguments in main() as a stringlist:

  <code>
  int main(int argc, char **argv)
  {
    stringlist *args = stringlist_new(argv)

    // ....
  }
  </code>

  It is not possible to set up a stringlist allocated on the stack.
 */
typedef struct {
	size_t   num;      /* number of actual strings */
	size_t   len;      /* number of memory slots for strings */
	char   **strings;  /* array of NULL-terminated strings */
} stringlist;

/**
  Callback function signature for sort comparisons.

  An sl_comparator function takes two arguments (adjacent strings
  in a stringlist) and returns an integer less than or greater than
  zero if the first value should come before or after the second.
 */
typedef int (*sl_comparator)(const void*, const void*);

#define for_each_string(l,i) for ((i)=0; (i)<(l)->num; (i)++)

int STRINGLIST_SORT_ASC(const void *a, const void *b);
int STRINGLIST_SORT_DESC(const void *a, const void *b);

#define SPLIT_NORMAL  0x00
#define SPLIT_GREEDY  0x01

stringlist* stringlist_new(char** src);
stringlist* stringlist_dup(stringlist *orig);
void stringlist_free(stringlist *list);
void stringlist_sort(stringlist *list, sl_comparator cmp);
void stringlist_uniq(stringlist *list);
int stringlist_search(const stringlist *list, const char *needle);
int stringlist_add(stringlist *list, const char *value);
int stringlist_add_all(stringlist *dst, const stringlist *src);
int stringlist_remove(stringlist *list, const char *value);
int stringlist_remove_all(stringlist *dst, stringlist *src);
stringlist *stringlist_intersect(const stringlist *a, const stringlist *b);
int stringlist_diff(stringlist *a, stringlist *b);
char* stringlist_join(stringlist *list, const char *delim);
stringlist* stringlist_split(const char *str, size_t len, const char *delim, int opt);

#endif /* STRINGLIST_H */
