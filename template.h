#ifndef _TEMPLATE_H
#define _TEMPLATE_H

#include "clockwork.h"
#include "hash.h"

enum tnode_type {
	TNODE_NOOP = 0,
	TNODE_ECHO,
	TNODE_VALUE,
	TNODE_REF,
	TNODE_IF_EQ,
	TNODE_FOR
};

struct tnode {
	enum tnode_type type;

	char *d1;
	char *d2;

	unsigned int size;
	struct tnode **nodes;
};

struct template {
	struct tnode *root;
	struct hash  *vars;
	struct hash  *facts; /* pointer to host facts; unmanaged */

	struct tnode **nodes;
	size_t nodes_len;
};

struct template* template_new(void);
struct template* template_create(const char *path, struct hash *facts);
void template_free(struct template *t);
int template_add_var(struct template *t, const char *name, const char *value);
void* template_deref_var(struct template *t, const char *name);
char* template_render(struct template *t);

struct tnode* template_new_tnode(struct template *t, enum tnode_type type, const char *d1, const char *d2);
int tnode_add(struct tnode *parent, struct tnode *child);

#endif /* _TEMPLATE_H */
