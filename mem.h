#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

/** @file mem.h

  This file defines some functions for more advanced and
  flexible memory management.

 */

/**
  Free a pointer and set it to NULL.

  @param  p    Pointer to free and nullify.
 */
#define xfree(p) __xfree((void **)&(p))

/** @cond false */
void __xfree(void **ptr2ptr);
/** @endcond */

/**
  Wrapper around strdup(3) that can handle NULL strings.

  @param  s    Pointer to string to duplicate.

  @returns a pointer to a duplicate copy of \a s if \a s is non-NULL,
           or NULL if \a s is NULL.
 */
char* xstrdup(const char *s);

/**
  Wrapper around strcmp(3) that can handle NULL strings.

  @param  a    First string to compare.
  @param  b    Second string to compare.

  @returns the results of strcmp(\a a, \a b) if both \a a and \a b are
           non-NULL, or -1 if either is NULL.
 */
int xstrcmp(const char *a, const char *b);

/**
  Duplicate a NULL-terminated array of character strings.

  Memory allocated by this function must be freed via an xarrfree call.

  @param  a    Pointer to the array to duplicate.

  @returns a pointer to a duplicate of the \a a array, or NULL if \a a
           could not be duplicated.
 */
char** xarrdup(char **a);

/**
  Free a NULL-terminated array of character strings.

  @param  a    Pointer to the array to free.
 */
void xarrfree(char **a);

#endif /* MEM_H */
