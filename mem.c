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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "mem.h"
#include "log.h"

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
		CRITICAL("%s, %s:%u - memory allocation failed: %s",
		      func, file, line, strerror(errno));
		/* If anyone wants to gripe about the exit call, please see:

		     http://www.reddit.com/comments/60vys/how_not_to_write_a_shared_library/
		     http://pulseaudio.org/ticket/158

		   I personally agree with lennart (PA author) */
		exit(42);
	}
	return buf;
}

char* xstrdup(const char *s)
{
	return (s ? strdup(s) : NULL);
}

int xstrcmp(const char *a, const char *b)
{
	return ((!a || !b) ? -1 : strcmp(a,b));
}

int streq(const char *a, const char *b)
{
	return xstrcmp(a,b) == 0;
}

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

char* string(const char *fmt, ...)
{
	char buf[256];
	char *buf2;
	size_t n;
	va_list args;

	va_start(args, fmt);
	n = vsnprintf(buf, 256, fmt, args) + 1;
	va_end(args);
	if (n > 256) {
		buf2 = xmalloc(n * sizeof(char));

		va_start(args, fmt);
		vsnprintf(buf2, n, fmt, args);
		va_end(args);
	} else {
		buf2 = strdup(buf);
	}

	return buf2;
}

