#ifndef STRINGLIST_H
#define STRINGLIST_H

#include <sys/types.h>

typedef struct {
	size_t   num;      /* number of strings in list */
	size_t   len;      /* number of available slots */

	char   **strings;  /* array of null-terminated strings */
} stringlist;

stringlist* stringlist_new(void);
void stringlist_free(stringlist*);

int stringlist_search(stringlist*, const char*);
int stringlist_add(stringlist*, const char*);
int stringlist_remove(stringlist*, const char*);

#endif /* STRINGLIST_H */
