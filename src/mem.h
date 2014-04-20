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

#ifndef MEM_H
#define MEM_H

#include "clockwork.h"
#include <stdlib.h>

/**
  Free $p and set it to NULL.
 */
#define xfree(p) __xfree((void **)&(p))

void __xfree(void **ptr2ptr);

/**
  Allocate and zero memory, with error checking.

  If $size bytes cannot be allocated, `xmalloc`
  issues a CRITICAL to log the failure and then
  exists with a recognizable status code.

  Otherwise, it returns a pointer to the memory.
 */
#define xmalloc(size) __xmalloc(size, __func__, __FILE__, __LINE__)

void* __xmalloc(size_t size, const char *func, const char *file, unsigned int line);

char* xstrdup(const char *s);
int xstrcmp(const char *a, const char *b);
char* xstrncpy(char *dest, const char *src, size_t n);

int streq(const char *a, const char *b);

char** xarrdup(char **ptr2arr);

/**
  Free $a, a NULL-terminated array of character strings.
 */
#define xarrfree(a) __xarrfree(&(a))

void __xarrfree(char ***a);

#endif
