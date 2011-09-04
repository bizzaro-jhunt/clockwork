/*
  Copyright 2011 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "hash.h"
#include "mem.h"

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
	return xmalloc(sizeof(struct hash));
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

void hash_free_all(struct hash *h)
{
	ssize_t i, j;
	for (i = 0; i < 64; i++) {
		for (j = 0; j < h->entries[i].len; j++) {
			free(h->entries[i].keys[j]);
			free(h->entries[i].values[j]);
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

/*
   Advance a hash_cursor until a key-value pair is found, or
   the end of the hash_list array is seen.
 */
static int _cursor_next(const struct hash *h, struct hash_cursor *c)
{
	assert(h); // LCOV_EXCL_LINE
	assert(c); // LCOV_EXCL_LINE

	const struct hash_list *hl;

	c->l2++;
	while (c->l1 < 64) {
		hl = &h->entries[c->l1];
		if (hl->len == 0 || c->l2 == hl->len) {
			c->l1++;
			c->l2 = 0;
			continue;
		}
		return 0;
	}

	return -1;
}

void *hash_next(const struct hash *h, struct hash_cursor *c, char **key, void **val)
{
	assert(h); assert(c); // LCOV_EXCL_LINE
	assert(key); assert(val); // LCOV_EXCL_LINE

	const struct hash_list *hl;

	*key = NULL;
	*val = NULL;
	if (_cursor_next(h, c) == 0) {
		hl = &h->entries[c->l1];
		*key = hl->keys[c->l2];
		*val = hl->values[c->l2];
	}

	return *key;
}

