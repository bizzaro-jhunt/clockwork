#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "mem.h"

void __xfree(void **ptr2ptr)
{
	//if (!ptr2ptr || !*ptr2ptr) { return; }
	if (!ptr2ptr) { return; }
	free(*ptr2ptr);
	*ptr2ptr = NULL;
}

char* xstrdup(const char *s)
{
	return (s ? strdup(s) : NULL);
}

int xstrcmp(const char *a, const char *b)
{
	return ((!a || !b) ? -1 : strcmp(a,b));
}

char** xarrdup(char **a)
{
	char **n, **t;

	if (!a) { return NULL; }
	for (t = a; *t; t++)
		;

	n = calloc(t - a + 1, sizeof(char*));
	if (!n) {
		return NULL;
	}

	for (t = n; *a; a++) {
		*t++ = xstrdup(*a);
	}

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
		buf2 = calloc(n, sizeof(char));
		if (!buf2) { return NULL; }

		va_start(args, fmt);
		vsnprintf(buf2, n, fmt, args);
		va_end(args);
	} else {
		buf2 = strdup(buf);
	}

	return buf2;
}

