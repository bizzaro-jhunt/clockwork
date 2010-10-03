#include <string.h>

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
