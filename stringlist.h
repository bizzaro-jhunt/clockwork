#ifndef STRINGLIST_H
#define STRINGLIST_H

#include <sys/types.h>

typedef struct {
	size_t   num;      /* number of strings in list */
	size_t   len;      /* number of available slots */

	char   **strings;  /* array of null-terminated strings */
} stringlist;

typedef int (*sl_comparator)(const void*, const void*);

#define STRINGLIST_SORT_ASC     _stringlist_strcmp_asc
#define STRINGLIST_SORT_DESC    _stringlist_strcmp_desc

int _stringlist_strcmp_asc(const void*, const void*);
int _stringlist_strcmp_desc(const void*, const void*);

stringlist* stringlist_new(char **src);
int stringlist_init(stringlist *list, char **src);
void stringlist_deinit(stringlist*);
void stringlist_free(stringlist*);

void stringlist_sort(stringlist*, sl_comparator);
void stringlist_uniq(stringlist*);

int stringlist_search(stringlist*, const char*);

int stringlist_add(stringlist*, const char*);
int stringlist_add_all(stringlist*, stringlist*);

int stringlist_remove(stringlist*, const char*);
int stringlist_remove_all(stringlist*, stringlist*);

int stringlist_diff(stringlist*, stringlist*);

char* stringlist_join(stringlist*, const char*);
stringlist* stringlist_split(const char*, size_t, const char*);

#endif /* STRINGLIST_H */
