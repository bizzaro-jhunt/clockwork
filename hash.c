#include "hash.h"

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
	free(h);
}

static void *lookup(struct hash_list *hl, const char *k)
{
	size_t i;
	for (i = 0; i < hl->len; i++) {
		if (strcmp(hl->keys[i], k) == 0) {
			return hl->values[i];
		}
	}
	return NULL;
}

void* hash_lookup(struct hash *h, const char *k)
{
	return lookup(&h->entries[H64(k)], k);
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

int hash_insert(struct hash *h, const char *k, void *v)
{
	unsigned char idx = H64(k);
	if (lookup(&h->entries[idx], k)) {
		return -1; /* can't insert, already exists */
	}

	return insert(&h->entries[idx], k, v);
}
