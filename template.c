#include "template.h"
#include "tpl/parser.h"

#include <ctype.h>

static void tnode_free(struct tnode *n)
{
	if (n) {
		free(n->d1);
		free(n->d2);
		free(n->nodes);
	}
	free(n);
}

struct template* template_new(void)
{
	struct template *t = xmalloc(sizeof(struct template));

	t->vars = hash_new();
	t->nodes = NULL;
	t->nodes_len = 0;

	t->root = template_new_tnode(t, TNODE_NOOP, NULL, NULL);

	return t;
}

struct template* template_create(const char *path, struct hash *facts)
{
	struct template *t = parse_template(path);
	t->facts = facts;
	return t;
}

void template_free(struct template *t)
{
	size_t i;

	if (t) {
		for (i = 0; i < t->nodes_len; i++) { tnode_free(t->nodes[i]); }
		free(t->nodes);
		free(t->vars);
		/* don't free t->facts; it's not ours */
	}
	free(t);
}

int template_add_var(struct template *t, const char *name, const char *value)
{
	hash_set(t->vars, name, xstrdup(value));
	return 0;
}

void* template_deref_var(struct template *t, const char *name)
{
	void *v;
	v = hash_get(t->vars, name);
	if (!v) { v = hash_get(t->facts, name); }
	return v;
}

/* FIXME: standalone implementation */
struct autostr2 {
	size_t len;
	size_t bytes;
	size_t block;

	char *data;
	char *ptr;
};

struct autostr2* autostr_new(const char *seed, size_t block);
int autostr_add_string(struct autostr2 *s, const char *str);
int autostr_add_char(struct autostr2 *s, char c);
//void autostr_dump(struct autostr2 *s);

struct autostr2* autostr_new(const char *seed, size_t block)
{
	struct autostr2 *s;

	if (block <= 0) { block = 64; }

	s = xmalloc(sizeof(struct autostr2));

	s->len = 0;
	s->bytes = s->block = block;

	s->data = xmalloc(sizeof(char) * s->block);
	s->ptr = s->data;

	autostr_add_string(s, seed);
	return s;
}

int autostr_add_string(struct autostr2 *s, const char *str)
{
	char *tmp;
	size_t new_bytes;

	if (!str) { return -1; }

	new_bytes = s->len + strlen(str);
	if (new_bytes >= s->bytes) {
		new_bytes = (new_bytes / s->block + 1) * s->block;
		tmp = realloc(s->data, new_bytes);
		if (!tmp) {
			return -1;
		}

		s->data = tmp;
		s->ptr = s->data + s->len; /* reset s->ptr, in case s->data moved) */
		s->bytes = new_bytes;
	}

	for (; *str; *s->ptr++ = *str++, s->len++)
		;
	*s->ptr = '\0';

	return 0;
}

#if 0
void autostr_dump(struct autostr2 *s)
{
	size_t i;

	for (i = 0; i < s->bytes; i++) {
		if (isprint(s->data[i])) {
			fprintf(stderr, "autostr[%u] = %c\n", i, s->data[i]);
		} else {
			fprintf(stderr, "autostr[%u] = 0x%x\n", i, s->data[i]);
		}
	}
}
#endif

int autostr_add_char(struct autostr2 *s, char c)
{
	char *tmp;
	size_t new_bytes = s->len + 1;
	if (new_bytes > s->bytes) {
		new_bytes = s->bytes + s->block;
		tmp = realloc(s->data, new_bytes);
		if (!tmp) {
			return -1;
		}

		s->data = tmp;
		s->bytes = new_bytes;
	}

	*s->ptr++ = c;
	s->len++;
	*s->ptr = '\0';

	return 0;
}

struct template_context {

	struct autostr2 *out;
	int echo;
};

static int _template_render(struct template *t, struct tnode *n, struct template_context *ctx)
{
	unsigned int i;

again:
	switch (n->type) {
	case TNODE_NOOP:
		break;

	case TNODE_ECHO:
		ctx->echo = 1;
		for (i = 0; i < n->size; i++) {
			_template_render(t, n->nodes[i], ctx);
		}
		ctx->echo = 0;
		return 0;

	case TNODE_VALUE:
		if (ctx->echo) {
			autostr_add_string(ctx->out, n->d1);
		}
		return 0;

	case TNODE_REF:
		if (ctx->echo) {
			autostr_add_string(ctx->out, template_deref_var(t, n->d1));
		}
		return 0;

	case TNODE_IF_EQ:
		if (xstrcmp(n->d2, template_deref_var(t, n->d1)) == 0) {
			n = n->nodes[0];
			goto again;
		} else if (n->nodes[1]) {
			n = n->nodes[1];
			goto again;
		}

	case TNODE_FOR:
		/* not yet implemented */
		return 0;

	default:
		ERROR("Bad node type: %i", n->type);
		return -1;
	}

	for (i = 0; i < n->size; i++) {
		_template_render(t, n->nodes[i], ctx);
	}
	return 0;
}

char* template_render(struct template *t)
{
	char *data;
	struct template_context ctx;
	ctx.echo = 0;
	ctx.out = autostr_new(NULL, 0);

	_template_render(t, t->root, &ctx);

	data = xstrdup(ctx.out->data);
	free(ctx.out->data);
	free(ctx.out);
	return data;
}

struct tnode* template_new_tnode(struct template *t, enum tnode_type type, const char *d1, const char *d2)
{
	struct tnode *node;
	struct tnode **nodes;

	node = xmalloc(sizeof(struct tnode));
	nodes = realloc(t->nodes, sizeof(struct node*) * (t->nodes_len + 1));

	if (!node || !nodes) {
		free(node);
		free(nodes);
		return NULL;
	}

	node->type = type;
	node->d1 = xstrdup(d1);
	node->d2 = xstrdup(d2);

	nodes[t->nodes_len++] = node;
	t->nodes = nodes;

	return node;
}

int tnode_add(struct tnode *parent, struct tnode *child)
{
	struct tnode **list;

	if (!parent || !child) { return -1; }

	list = realloc(parent->nodes, sizeof(struct tnode*) * (parent->size + 1));
	if (!list) { return -1; }

	list[parent->size++] = child;
	parent->nodes = list;

	return 0;
}

