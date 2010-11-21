#ifndef _HASH_H
#define _HASH_H

#include <sys/types.h>

struct hash_list {
	char **keys;
	void **values;
	ssize_t len;
};

//typedef unsigned char (*hashf)(const char*);

struct hash {
	struct hash_list entries[64];
};

unsigned char H64(const char *s);

struct hash *hash_new(void);
void hash_free(struct hash *h);

void* hash_get(const struct hash *h, const char *k);
void* hash_set(struct hash *h, const char *k, void *v);

#endif
