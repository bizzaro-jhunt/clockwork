#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "gear.h"
#include "conffile.h"

struct hash *parse_config(const char *path)
{
	FILE *io = fopen(path, "r");
	if (!io) return NULL;

	struct hash *h = hash_new();
	char buf[8192];
	char *k, *v, *t;

	while (fgets(buf, 8192, io)) {
		for (k = buf; *k && isspace(*k); k++);
		if (!*k || *k == '#') continue;
		fprintf(stderr, "k = '%s'\n", k);

		for (v = k; *v && !(isspace(*v) || *v == '='); v++);
		if (!*v) goto fail;
		if (*v == '=') {
			*v++ = '\0';
		} else {
			for (*v++ = '\0'; *v && *v != '='; v++);
			if (!*v) goto fail;
			for (v++; *v && isspace(*v); v++);
		}
		if (!*v || *v == '#') goto fail;
		if (*v == '"') {
			v++;
			for (t = v; *t && *t != '"'; t++);
			if (!*t) goto fail;
			*t = '\0';
		}
		for (t = v; *t && *t != '\n'; t++);
		*t = '\0';

		hash_set(h, k, strdup(v));
	}
	return h;

fail:
	fprintf(stderr, "failed!\n");
	return NULL;
}
