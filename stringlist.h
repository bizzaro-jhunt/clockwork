#ifndef STRINGLIST_H
#define STRINGLIST_H

#include <sys/types.h>

/** @file stringlist.h

  A stringlist is a specialized data structure designed for
  efficient storage and management of a list of strings.  It
  wraps around the C representation as a NULL-terminated array
  of character pointers, and extends the idiom with memory
  management methods and "standard" string list manipulation
  routines.

  <h3>Basic Usage Scenarios</h3>

  Stringlist allocation and de-allocation is performed through
  the stringlist_new and stringlist_free functions.  These two
  functions allocate memory from the heap, and then free that
  memory.

  @verbatim
  // allocate a new, empty stringlist
  stringlist *lst = stringlist_new(NULL);

  // do something interesting with the list...
  stringlist_add(fruit, "pear");
  stringlist_add(fruit, "apple");
  stringlist_add(fruit, "cantaloupe");

  // free memory allocated by stringlist_new and stringlist_add
  stringlist_free(lst);
  @endverbatim

  The first argument to stringlist_new is a C-style char **
  string array, and can be used to "seed" the stringlist with
  starting values.  For example, to deal with command-line
  arguments in main() as a stringlist:

  @verbatim
  int main(int argc, char **argv)
  {
    stringlist *args = stringlist_new(argv)

    // ....
  }
  @endverbatim

  It is not possible to set up a stringlist allocated on the stack.

 */

/**
  A list of strings.
 */
typedef struct {
	/** Number of strings in list */
	size_t   num;

	/** Number of available slots */
	size_t   len;

	/** Array of null-terminated strings */
	char   **strings;
} stringlist;

/**
  Callback function signature for sort comparisons.

  An sl_comparator function takes two arguments (adjacent strings
  in a stringlist) and returns an integer less than or greater than
  zero if the first value should come before or after the second.
 */
typedef int (*sl_comparator)(const void*, const void*);

/** @cond false */
int STRINGLIST_SORT_ASC(const void *a, const void *b);
int STRINGLIST_SORT_DESC(const void *a, const void *b);
/** @endcond */

/**
  Allocate and initialize a new stringlist.

  This function allocates memory for a new stringlist, and then
  initializes the structure.  If the \a src parameter is a NULL pointer,
  the new list will be empty.  Otherwise, the contents of \a src will
  be copied into the stringlist structure.

  If problems are encountered (i.e. while allocating memory),
  stringlist_new will free any memory that was successfully
  allocated and then return NULL.

  @note  Callers are responsible for freeing the memory allocated by this
  call, and should use stringlist_free for that.

  @param  src    A traditional C-style array of NULL-terminated character
                 strings, to be used as an initial list.

  @returns a pointer to a stringlist allocated on the heap, or NULL on failure.
 */
stringlist* stringlist_new(const char * const *src);

/**
  Free memory allocated by a stringlist.

  This function should be called whenever a stringlist is no longer
  necessary.

  @param  list    A pointer to a stringlist, returned by stringlist_new.
 */
void stringlist_free(stringlist *list);

/**
  Sort the list of strings, according to a comparison function.

  Comparison functions operate according to existing convention;  For each
  pair of adjacent strings, the function will be called and passed both
  strings.  It should return an integer less than, or greater than zero if
  the first string should appear before or after the second.  If both
  strings are equal, the comparator should return 0, and the sort order
  will be undefined.

  Two basic comparators are defined already: STRINGLIST_SORT_ASC and
  STRINGLIST_SORT_DESC for sorting alphabetically and reverse alphabetically,
  respectively.

  @note  The sort is done in-place; that is, the stringlist passed to
         stringlist_sort \b will \b be modified.

  @param  list    A pointer to a stringlist to sort.
  @param  cmp     Comparator function.
 */
void stringlist_sort(stringlist *list, sl_comparator cmp);

/**
  Remove duplicate strings from a list.

  This function will reduce a stringlist such that it only contains
  unique values.  In order to do that, the stringlist will first be
  sorted (using stringlist_sort and STRINGLIST_SORT_ASC), so that
  duplicate values are adjacent.

  @note  De-duplication is done in-place; the stringlist passed \b will
         \b be modified.

  Example:

  @verbatim
  stringlist *fruit = stringlist_new(NULL);

  stringlist_add(fruit, "pear");
  stringlist_add(fruit, "banana");
  stringlist_add(fruit, "pear");
  stringlist_add(fruit, "apple");
  stringlist_add(fruit, "pear");

  // at this point, the list is 'pear', 'banana', 'pear', 'apple', 'pear'

  stringlist_uniq(fruit);

  // now, the list is 'apple', 'banana', 'pear'

  @endverbatim

  @param  list    A pointer to a stringlist to remove duplicates from.
 */
void stringlist_uniq(stringlist *list);

/**
  Search a stringlist for a specific value.

  @param  list      A pointer to the stringlist to search.
  @param  needle    String to search for.

  @returns 0 if \a needle exists in \a list, or a value less than zero
           if it doesn't.
 */
int stringlist_search(const stringlist *list, const char *needle);

/**
  Add a string to a stringlist.

  @note This function makes a copy of \a value (via strdup) and
        adds this to the list.  Memory management of the pointer
        passed for \a value is the responsibility of the caller.

  @param  list     A pointer to the stringlist to add to.
  @param  value    String to add.

  @returns 0 if \a value was added to \a list; non-zero if not.
 */
int stringlist_add(stringlist *list, const char *value);

/**
  Combine two stringlists.

  This function is used to append one stringlist onto the end of
  another.  The act of concatenation itself is guaranteed to be
  atomic; if stringlist_add_all fails, \a dst is untouched.

  The order of the arguments can be remembered by envisioning the
  call as a replacement for a simpler one: \a dst += \a src

  @param  dst    Stringlist to append to.
  @param  src    Stringlist to add to \a dst.

  @returns 0 if all strings from \a src were added to \a dst
           successfully, or non-zero if not.
 */
int stringlist_add_all(stringlist *dst, const stringlist *src);

/**
  Remove the first occurrence of \a value from \a list.

  If \a value appears multiple times in the stringlist, then
  multiple calls need to be made to remove all of them, best
  done in a while loop:

  @verbatim
  while (stringlist_remove(sl, "a string") != 0)
      ;
  @endverbatim

  @param  list     List to remove \a value from.
  @param  value    String value to remove.

  @returns 0 if \a value was found in, and removed from, \a list,
           or non-zero if not.
 */
int stringlist_remove(stringlist *list, const char *value);

/**
  Remove one stringlist's values from another.

  The order of the arguments can be remembered by thinking of this
  call as in terms of a simpler one: \a dst -= \a src

  @note stringlist_remove_all deals with duplicate values naively,
        only removing the first such occurrence in \a dst for each
        occurrence in \a src.

  @verbatim
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
  @endverbatim

  @param  dst    List to remove values from.
  @param  src    List of strings to remove from \a dst.

  @returns 0 if values present in both \a src and \a dst were
           removed from \a dst, or non-zero if not.
 */
int stringlist_remove_all(stringlist *dst, stringlist *src);

/**
  Compare two stringlists, looking for equivalency.

  @param  a    A pointer to a stringlist.
  @param  b    A pointer to a stringlist.

  @returns 0 if \a a and \a b are equivalent and represent the same
           list of strings; or non-zero if not.
 */
int stringlist_diff(stringlist *a, stringlist *b);

/**
  Join the strings in a stringlist, forming a single string.

  This function works similarly to Perl's join function and Ruby's
  String#join method.

  Examples:

  @verbatim
  stringlist *l = stringlist_new(NULL);
  stringlist_add(l, "red");
  stringlist_add(l, "green");
  stringlist_add(l, "blue");

  // A few different ways to join stringlists:
  stringlist_join(l, ", ");    // "red, green, blue"
  stringlist_join(l, " and "); // "red and green and blue"
  stringlist_join(l, ":");     // "red:green:blue"
  @endverbatim

  For the special case of an empty list, stringlist_join will return
  an empty string ("") no matter what \a delim is.

  @param  list     List of strings to join.
  @param  delim    String delimiter for separating string components.

  @returns a heap-allocated string containing each string in \a list,
           separated by \a delim, or NULL if such a string could not
           be created.
 */
char* stringlist_join(stringlist *list, const char *delim);

/**
  Split a string into tokens, stored in a stringlist.

  This function works similarly to Perl's split function and Ruby's
  String#split method.

  Examples:

  @verbatim
  stringlist *l = stringlist_split("one::two::three", 15, "::");
  // List l now contains three strings: 'one', 'two', and 'three'
  @endverbatim

  @param  str      The string to split into tokens.
  @param  len      Length of \a str.
  @param  delim    Delimiter to split string on (can be more than
                   one character).

  @returns a heap-allocated stringlist containing the split tokens,
           or NULL on failure.
 */
stringlist* stringlist_split(const char *str, size_t len, const char *delim);

#endif /* STRINGLIST_H */
