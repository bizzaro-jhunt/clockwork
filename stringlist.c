#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "stringlist.h"

#define INIT_LEN   16
#define EXPAND_LEN  8

#define _stringlist_full(sl) ((sl)->num == (sl)->len - 1)
static int _stringlist_expand(stringlist*, size_t);

static int _stringlist_expand(stringlist *sl, size_t expand)
{
	assert(sl);
	assert(expand > 0);

	char **s;

	expand += sl->len;
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

stringlist* stringlist_new(void)
{
	stringlist *sl;

	sl = malloc(sizeof(stringlist));
	if (!sl) {
		return NULL;
	}

	sl->num = 0;
	sl->len = INIT_LEN;
	sl->strings = calloc(sl->len, sizeof(char *));
	if (!sl->strings) {
		free(sl);
		return NULL;
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
	if (_stringlist_full(sl) && _stringlist_expand(sl, EXPAND_LEN) != 0) {
		return -1;
	}

	sl->strings[sl->num] = strdup(str);
	sl->num++;
	sl->strings[sl->num] = NULL;

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
