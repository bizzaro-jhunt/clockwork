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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "mem.h"
#include "cw.h"

void __xfree(void **ptr2ptr)
{
	if (!ptr2ptr) { return; }
	free(*ptr2ptr);
	*ptr2ptr = NULL;
}

void* __xmalloc(size_t size, const char *func, const char *file, unsigned int line)
{
	void *buf = calloc(1, size);
	if (!buf) {
		cw_log(LOG_CRIT, "%s, %s:%u - memory allocation failed: %s",
		      func, file, line, strerror(errno));
		/* If anyone wants to gripe about the exit call, please see:

		     http://www.reddit.com/comments/60vys/how_not_to_write_a_shared_library/
		     http://pulseaudio.org/ticket/158

		   I personally agree with lennart (PA author) */
		exit(42);
	}
	return buf;
}

/**
  Duplicate $s, and gracefully handle NULL strings.

  Returns a pointer to a duplicate copy of $s, unless
  $s is NULL (in which case, returns NULL).
 */
char* xstrdup(const char *s)
{
	return (s ? strdup(s) : NULL);
}

/**
  Compare $a and $b, handling NULL strings.

  If either string is NULL, returns -1.
  Otherwise, returns the value of strcmp($a,$b).
  See `strcmp(3)`.
 */
int xstrcmp(const char *a, const char *b)
{
	return ((!a || !b) ? -1 : strcmp(a,b));
}

int streq(const char *a, const char *b)
{
	return xstrcmp(a,b) == 0;
}

/**
  Copy $src into $dest.

  This function behaves like `strcy(3)`, except that it
  properly handles NULL strings for both $src and $dest
  (in which case it returns NULL).  It also properly
  NULL-terminates $dest.
 */
char* xstrncpy(char *dest, const char *src, size_t n)
{
	/* Note: size_t is unsigned, so n can never be < 0 */
	if (!dest || !src || n == 0) {
		return NULL;
	}

	dest = strncpy(dest, src, n);
	dest[n-1] = '\0';
	return dest;
}

/**
  Duplicate $a, a NULL-terminated array of character strings.

  Memory allocated by this function must be freed via @xarrfree.

  On success, returns a pointer to a copy of $a.
  On failure, returns NULL.
 */
char** xarrdup(char **a)
{
	char **n, **t;

	if (!a) { return NULL; }
	for (t = a; *t; t++)
		;

	n = xmalloc((t -a + 1) * sizeof(char*));
	for (t = n; *a; a++)
		*t++ = xstrdup(*a);

	return n;
}

void __xarrfree(char ***a)
{
	char **s;
	if (!a || !*a) { return; }
	s = *a;
	while (*s) {
		free(*s++);
	}
	free(*a);
	*a = NULL;
}

