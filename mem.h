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
  Wrapper around malloc(3)/calloc(3) that exits
  if memory cannot be allocated.

  @param size  Number of bytes to allocate

  @returns a pointer to the newly allocated chunk of memory,
           or issues a CRITICAL and exits (42) on failure.
 */
#define xmalloc(size) __xmalloc(size, __func__, __FILE__, __LINE__)

/** @cond false */
void* __xmalloc(size_t size, const char *func, const char *file, unsigned int line);
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

int streq(const char *a, const char *b);

/**
  Copy one string into another buffer.

  Unlike the standard libc +strcpy+ function that it wraps, strncpy
  correctly handles NULL \a dest and \a src parameters, and places
  a NULL-terminator in \a dest.

  @param  dest   Buffer to copy into.
  @param  src    String buffer to copy from.
  @param  n      Number of bytes to copy from \a src into \a dest.

  @returns  The \a dest parameter (just like standard +strncpy+)
 */
char* xstrncpy(char *dest, const char *src, size_t n);

/**
  Duplicate a NULL-terminated array of character strings.

  Memory allocated by this function must be freed via an xarrfree call.

  @param  ptr2arr  Pointer to the array to duplicate.

  @returns a pointer to a duplicate of the \a ptr2arr array, or NULL
           if \a ptr2arr could not be duplicated.
 */
char** xarrdup(char **ptr2arr);

/**
  Free a NULL-terminated array of character strings.

  @param  a    Pointer to the array to free.
 */
#define xarrfree(a) __xarrfree(&(a))
/** @cond false */
void __xarrfree(char ***a);
/** @endcond */

/**
  Build an arbitrary string, with printf-like flexibility

  For formatting options support, see printf(3).

  The returned string must be freed by the caller.

  @param  fmt   A printf-like format string.
  @param  ...   Variable arguments for interpolating \a fmt.

  @returns a dynamically allocated string containing the interpolated
           \a fmt string.
 */
char* string(const char *fmt, ...);

#endif /* MEM_H */
