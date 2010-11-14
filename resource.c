#include "resource.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char* resource_key(const char *type, const char *id)
{
	assert(type);
	assert(id);

	char *key;
	size_t keylen;

	/* type + ":" + id */
	keylen = strlen(type) + 1 + strlen(id);
	key = malloc((keylen + 1) * sizeof(char));
	if (key) {
		snprintf(key, keylen + 1, "%s:%s", type, id);
	}

	return key;
}

