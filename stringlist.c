#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "stringlist.h"

#define INIT_LEN   16

#define EXPAND_FACTOR 8
#define EXPAND_LEN(x) (x / EXPAND_FACTOR + 1) * EXPAND_FACTOR

static int _stringlist_expand(stringlist*, size_t);
static int _stringlist_reduce(stringlist*);
static size_t _stringlist_capacity(stringlist*);

static int _stringlist_expand(stringlist *sl, size_t expand)
{
	assert(sl);
	assert(expand > 0);

	char **s;
	expand = EXPAND_LEN(expand) + sl->len;
	s = realloc(sl->strings, expand * sizeof(char *));
	if (!s) {
		return -1;
	}

	sl->strings = s;
	for (; sl->len < expand; sl->len++) {
		sl->strings[sl->len] = NULL;
	}

	return 0;
}

/**
 * Walk the list and zip up NULL strings
 */
static int _stringlist_reduce(stringlist *sl)
{
	char **ins; char **ptr, **end;

	ptr = ins = sl->strings;
	end = sl->strings + sl->num;
	while (ins < end) {
		while (!*ptr && ptr++ < end) {
			sl->num--;
		}

		if (ptr == end) {
			break;
		}

		*ins++ = *ptr++;
	}

	return 0;
}

static size_t _stringlist_capacity(stringlist* sl)
{
	return sl->len - 1 - sl->num;
}

/*****************************************************************/

int _stringlist_strcmp_asc(const void *a, const void *b)
{
	/* params are pointers to char* */
	return strcmp(* (char * const *) a, * (char * const *) b);
}

int _stringlist_strcmp_desc(const void *a, const void *b)
{
	/* params are pointers to char* */
	return -1 * strcmp(* (char * const *) a, * (char * const *) b);
}

stringlist* stringlist_new(char **src)
{
	stringlist *sl;
	char **t;

	sl = malloc(sizeof(stringlist));
	if (!sl) {
		return NULL;
	}

	if (src) {
		t = src;
		while (*t++)
			;
		sl->num = t - src - 1;
		sl->len = EXPAND_LEN(sl->num);
	} else {
		sl->num = 0;
		sl->len = INIT_LEN;
	}

	sl->strings = calloc(sl->len, sizeof(char *));
	if (!sl->strings) {
		free(sl);
		return NULL;
	}

	if (src) {
		for (t = sl->strings; *src; src++, t++) {
			*t = strdup(*src);
		}
	}

	return sl;
}

void stringlist_free(stringlist *sl)
{
	assert(sl);

	size_t i;

	for (i = 0; i < sl->num; i++) {
		free(sl->strings[i]);
	}

	free(sl->strings);
	free(sl);
}

void stringlist_sort(stringlist* sl, sl_comparator cmp)
{
	assert(sl);
	assert(cmp);

	if (sl->num < 2) { return; }
	qsort(sl->strings, sl->num, sizeof(char *), cmp);
}

void stringlist_uniq(stringlist *sl)
{
	assert(sl);

	size_t i;

	if (sl->num < 2) { return; }

	stringlist_sort(sl, STRINGLIST_SORT_ASC);
	for (i = 0; i < sl->num - 1; i++) {
		if (strcmp(sl->strings[i], sl->strings[i+1]) == 0) {
			free(sl->strings[i]);
			sl->strings[i] = NULL;
		}
	}
	_stringlist_reduce(sl);
}

int stringlist_search(stringlist *sl, const char* needle)
{
	assert(sl);
	assert(needle);

	size_t i;
	for (i = 0; i < sl->num; i++) {
		if ( strcmp(sl->strings[i], needle) == 0 ) {
			return 0;
		}
	}
	return -1;
}

int stringlist_add(stringlist *sl, const char* str)
{
	assert(sl);
	assert(str);

	/* expand as needed */
	if (_stringlist_capacity(sl) == 0 && _stringlist_expand(sl, 1) != 0) {
		return -1;
	}

	sl->strings[sl->num++] = strdup(str);
	sl->strings[sl->num] = NULL;

	return 0;
}

int stringlist_add_all(stringlist *dst, stringlist *src)
{
	assert(src);
	assert(dst);

	size_t i;

	if (_stringlist_capacity(dst) < src->num && _stringlist_expand(dst, src->num)) {
		return -1;
	}

	for (i = 0; i < src->num; i++) {
		dst->strings[dst->num++] = strdup(src->strings[i]);
	}
	dst->strings[dst->num] = NULL;

	return 0;
}

int stringlist_remove(stringlist *sl, const char *str)
{
	assert(sl);
	assert(str);

	char *removed = NULL;
	size_t i;
	for (i = 0; i < sl->num; i++) {
		if (strcmp(sl->strings[i], str) == 0) {
			removed = sl->strings[i];
			break;
		}
	}

	for (; i < sl->num; i++) {
		sl->strings[i] = sl->strings[i+1];
	}

	if (removed) {
		sl->num--;
		free(removed);
		return 0;
	}

	return -1;
}

int stringlist_remove_all(stringlist *dst, stringlist *src)
{
	assert(src);
	assert(dst);

	size_t s, d;
	for (d = 0; d < dst->num; d++) {
		for (s = 0; s < src->num; s++) {
			if ( strcmp(dst->strings[d], src->strings[s]) == 0 ) {
				free(dst->strings[d]);
				dst->strings[d] = NULL;
				break;
			}
		}
	}

	return _stringlist_reduce(dst);
}

int stringlist_diff(stringlist* a, stringlist* b)
{
	assert(a);
	assert(b);

	size_t i;

	if (a->num != b->num) { return 0; }
	for (i = 0; i < a->num; i++) {
		if (stringlist_search(b, a->strings[i]) != 0
		 || stringlist_search(a, b->strings[i]) != 0) {
			return 0;
		}
	}

	return -1; /* equivalent */
}

char* stringlist_join(stringlist *list, const char *delim)
{
	assert(list);
	assert(delim);

	size_t i, len = 0, delim_len = strlen(delim);
	char *joined, *ptr;

	if (list->num == 0) {
		return strdup("");
	}

	for (i = 0; i < list->num; i++) {
		len += strlen(list->strings[i]);
	}
	len += (list->num - 1) * delim_len;

	joined = malloc(len + 1);
	if (!joined) {
		return NULL;
	}

	ptr = joined;
	for (i = 0; i < list->num; i++) {
		if (i != 0) {
			memcpy(ptr, delim, delim_len);
			ptr += delim_len;
		}

		len = strlen(list->strings[i]);
		memcpy(ptr, list->strings[i], len);
		ptr += len;
	}
	*ptr = '\0';

	return joined;
}

stringlist* stringlist_split(const char *str, size_t len, const char *delim)
{
	stringlist *list = stringlist_new(NULL);
	char *a, *b, *end = str + len;
	size_t delim_len = strlen(delim);
	char *item;

	a = str;
	while (a < end) {
		for (b = a; b < end; b++) {
			if (strncmp(b, delim, delim_len) == 0) {
				break;
			}
		}

		item = malloc(b - a + 1);
		if (!item) {
			stringlist_free(list);
			return NULL;
		}
		memcpy(item, a, b - a);
		item[b-a] = '\0';

		if (stringlist_add(list, item) != 0) {
			stringlist_free(list);
			return NULL;
		}

		free(item);
		a = b + delim_len;
	}

	return list;
}

