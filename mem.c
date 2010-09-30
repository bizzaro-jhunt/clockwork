#include <string.h>

#include "mem.h"

void __xfree(void **ptr2ptr)
{
	//if (!ptr2ptr || !*ptr2ptr) { return; }
	if (!ptr2ptr) { return; }
	free(*ptr2ptr);
	*ptr2ptr = NULL;
}

char* xstrdup(const char *s) {
	return (s ? strdup(s) : NULL);
}
