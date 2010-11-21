#include "hash.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static unsigned char H256(const char *s)
{
	unsigned int h = 81;
	unsigned char c;

	while ((c = *s++))
		h = ((h << 5) + h) + c;

	return h;
}

unsigned char H64(const char *s)
{
	return H256(s) &~0xc0;
}

struct hash *hash_new(void)
{
	return calloc(1, sizeof(struct hash));
}

void hash_free(struct hash *h)
{
	ssize_t i, j;
	for (i = 0; i < 64; i++) {
		for (j = 0; j < h->entries[i].len; j++) {
			free(h->entries[i].keys[j]);
		}
		free(h->entries[i].keys);

		free(h->entries[i].values);
	}
	free(h);
}

static ssize_t get_index(const struct hash_list *hl, const char *k)
{
	ssize_t i;
	for (i = 0; i < hl->len; i++) {
		if (strcmp(hl->keys[i], k) == 0) {
			return i;
		}
	}
	return (ssize_t)-1;
}

static int insert(struct hash_list *hl, const char *k, void *v)
{
	char **new_k;
	void **new_v;

	new_k = realloc(hl->keys,   (hl->len + 1) * sizeof(char*));
	new_v = realloc(hl->values, (hl->len + 1) * sizeof(void*));

	/* FIXME check new_k / new_v for NULL */
	new_k[hl->len] = strdup(k);
	new_v[hl->len] = v;

	hl->keys   = new_k;
	hl->values = new_v;
	hl->len++;

	return 0;
}

void* hash_get(const struct hash *h, const char *k)
{
	ssize_t i;
	const struct hash_list *hl;

	if (!h || !k) { return NULL; }

	hl = &h->entries[H64(k)];
	i = get_index(hl, k);
	return (i < 0 ? NULL : hl->values[i]);
}

void* hash_set(struct hash *h, const char *k, void *v)
{
	ssize_t i;
	void *existing;
	struct hash_list *hl;

	if (!h || !k) { return NULL; }

	hl = &h->entries[H64(k)];
	i = get_index(hl, k);

	if (i < 0) {
		insert(hl, k, v);
		return v;
	} else {
		existing = hl->values[i];
		hl->values[i] = v;
		return existing;
	}
}
