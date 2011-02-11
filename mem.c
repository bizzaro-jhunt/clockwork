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

char* xstrncpy(char *dest, const char *src, size_t n)
{
	if (!dest || !src || n <= 0) {
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

void xarrfree(char **a)
{
	char **s = a;
	if (!a) { return; }
	while (*s) {
		free(*s++);
	}
	free(a);
}

char* string(const char *fmt, ...)
{
	char buf[256];
	char *buf2;
	size_t n;
	va_list args;

	va_start(args, fmt);
	n = vsnprintf(buf, 256, fmt, args);
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

