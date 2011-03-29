#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "policy.h"
#include "pack.h"
#include "resource.h"
#include "stringlist.h"

#define POLICY_PACK_PREFIX "policy::"
#define POLICY_PACK_OFFSET 8
/* Pack format for policy structure (first line):
     a - name
 */
#define POLICY_PACK_FORMAT "a"

/** @cond false */
struct policy_generator {
	struct policy *policy;
	struct hash   *facts;
	enum restype   type;

	struct resource *res;
};
/** @endcond */

static int _policy_normalize(struct policy *pol)
{
	assert(pol);

	struct resource *r;

	for_each_node(r, &pol->resources, l) {
		if (resource_norm(r) != 0) { return -1; }
	}

	return 0;
}

/* Allocate and initialize a new manifest structure */
struct manifest* manifest_new(void)
{
	struct manifest *m;

	m = xmalloc(sizeof(struct manifest));
	m->policies = hash_new();
	m->hosts    = hash_new();

	m->nodes = NULL;
	m->nodes_len = 0;

	m->root = manifest_new_stree(m, PROG, NULL, NULL);

	return m;
}

/* Free a manifest structure, and its allocated storage */
void manifest_free(struct manifest *m)
{
	size_t i;

	if (m) {
		for (i = 0; i < m->nodes_len; i++) {
			free(m->nodes[i]->data1);
			free(m->nodes[i]->data2);
			free(m->nodes[i]->nodes);
			free(m->nodes[i]);
		}
		free(m->nodes);

		hash_free(m->policies);
		hash_free(m->hosts);
	}
	free(m);
}

struct stree* manifest_new_stree(struct manifest *m, enum oper op, const char *data1, const char *data2)
{
	struct stree *stree;
	struct stree **list;

	stree = xmalloc(sizeof(struct stree));
	list = realloc(m->nodes, sizeof(struct stree*) * (m->nodes_len + 1));
	if (!stree || !list) {
		free(stree);
		free(list);
		return NULL;
	}

	stree->op = op;
	stree->data1 = xstrdup(data1);
	stree->data2 = xstrdup(data2);

	list[m->nodes_len++] = stree;
	m->nodes = list;

	return stree;
}

int stree_add(struct stree *parent, struct stree *child)
{
	assert(parent);

	struct stree **list;

	list = realloc(parent->nodes, sizeof(struct stree*) * (parent->size + 1));
	if (!list || !child) {
		free(list);
		return -1;
	}

	list[parent->size++] = child;
	parent->nodes = list;

	return 0;
}

int stree_compare(const struct stree *a, const struct stree *b)
{
	unsigned int i;

	if (!a || !b) {
		return 1;
	}

	if (a->op != b->op
	 || a->size != b->size
	 || (a->data1 && !b->data1) || (!a->data1 && b->data1)
	 || (a->data2 && !b->data2) || (!a->data2 && b->data2)
	 || (a->data1 && strcmp(a->data1, b->data1) != 0)
	 || (a->data2 && strcmp(a->data2, b->data2) != 0)) {
		return 1;
	}

	for (i = 0; i < a->size; i++) {
		if (stree_compare(a->nodes[i], b->nodes[i]) != 0) {
			return 1;
		}
	}

	return 0;
}

int fact_parse(const char *line, char **k, char **v)
{
	assert(line);

	char *buf, *name, *value;
	char *stp; /* string traversal pointer */

	buf = strdup(line);

	stp = buf;
	for (name = stp; *stp && *stp != '='; stp++)
		;
	*stp++ = '\0';
	for (value = stp; *stp && *stp != '\n'; stp++)
		;
	*stp = '\0';

	*k = xstrdup(name);
	*v = xstrdup(value);

	free(buf);
	return 0;
}

struct hash* fact_read(FILE *io, struct hash *facts)
{
	assert(io);

	char buf[8192] = {0};
	char *k, *v;
	int allocated = 0;

	if (!facts) {
		facts = hash_new();
		allocated = 1;
	}

	if (!facts) {
		return NULL;
	}

	while (!feof(io)) {
		errno = 0;
		if (!fgets(buf, 8192, io)) {
			if (errno != 0) {
				if (allocated) {
					hash_free(facts);
				}
				facts = NULL;
			}
			break;
		}

		if (fact_parse(buf, &k, &v) == 0) {
			hash_set(facts, k, v);
			free(k);
		}
	}

	return facts;
}

static struct resource * _policy_find_resource(struct policy_generator *pgen, const char *type, const char *id)
{
	assert(pgen);

	char *key;
	struct resource *r = NULL;

	if ((key = string("res_%s:%s", type, id)) != NULL) {
		r = hash_get(pgen->policy->index, key);
		free(key);
	}

	return r;
}

static int _policy_generate(struct stree *node, struct policy_generator *pgen)
{
	assert(node);
	assert(pgen);

	unsigned int i;

again:
	switch(node->op) {
	case IF:
		if (xstrcmp(hash_get(pgen->facts, node->data1), node->data2) == 0) {
			node = node->nodes[0];
		} else {
			node = node->nodes[1];
		}
		goto again;

	case RESOURCE:
		pgen->res = _policy_find_resource(pgen, node->data1, node->data2);
		if (!pgen->res) {
			pgen->res = resource_new(node->data1, node->data2);
		}

		if (!pgen->res) {
			WARNING("Definition for unknown resource type '%s'", node->data1);
		} else {
			policy_add_resource(pgen->policy, pgen->res);
		}

		break;

	case ATTR:
		if (pgen->res) {
			resource_set(pgen->res, node->data1, node->data2);
		} else {
			WARNING("Attribute %s = '%s' defined for unknown type",
			        node->data1, node->data2);
		}

		break;

	case PROG:
	case NOOP:
		/* do nothing */
		break;

	default:
		WARNING("unexpected node in syntax tree: (%u %s/%s)", node->op, node->data1, node->data2);
		break;

	}

	for (i = 0; i < node->size; i++) {
		if (_policy_generate(node->nodes[i], pgen) != 0) {
			return -1;
		}
	}

	return 0;
}

struct policy* policy_generate(struct stree *root, struct hash *facts)
{
	assert(root);

	struct policy_generator pgen;

	pgen.facts = facts;
	pgen.policy = policy_new(root->data1);

	if (_policy_generate(root, &pgen) != 0) {
		policy_free(pgen.policy);
		return NULL;
	}

	_policy_normalize(pgen.policy);

	return pgen.policy;
}

struct policy* policy_new(const char *name)
{
	struct policy *pol;

	pol = xmalloc(sizeof(struct policy));
	pol->name = xstrdup(name);

	list_init(&pol->resources);
	pol->index = hash_new();

	return pol;
}

void policy_free(struct policy *pol)
{
	if (pol) {
		hash_free(pol->index);
		free(pol->name);
	}
	free(pol);
}

void policy_free_all(struct policy *pol)
{
	struct resource *r, *r_tmp;

	for_each_node_safe(r, r_tmp, &pol->resources, l) { resource_free(r); }
	policy_free(pol);
}

int policy_add_resource(struct policy *pol, struct resource *res)
{
	assert(pol);
	assert(res);

	list_add_tail(&res->l, &pol->resources);
	hash_set(pol->index, res->key, res);
	return 0;
}

char* policy_pack(const struct policy *pol)
{
	assert(pol);

	char *packed = NULL;
	stringlist *pack_list;
	struct resource *r;

	pack_list = stringlist_new(NULL);
	if (!pack_list) {
		return NULL;
	}

	packed = pack(POLICY_PACK_PREFIX, POLICY_PACK_FORMAT, pol->name);
	if (stringlist_add(pack_list, packed) != 0) {
		goto policy_pack_failed;
	}
	xfree(packed);

	for_each_node(r, &pol->resources, l) {
		packed = resource_pack(r);
		if (!packed || stringlist_add(pack_list, packed) != 0) {
			goto policy_pack_failed;
		}
		xfree(packed);
	}

	packed = stringlist_join(pack_list, "\n");
	stringlist_free(pack_list);

	return packed;

policy_pack_failed:
	xfree(packed);
	stringlist_free(pack_list);
	return NULL;
}

struct policy* policy_unpack(const char *packed_policy)
{
	assert(packed_policy);

	struct policy *pol;
	stringlist *pack_list;
	char *packed;
	size_t i;

	char *pol_name;
	struct resource *r;

	pack_list = stringlist_split(packed_policy, strlen(packed_policy), "\n");
	if (!pack_list) {
		return NULL;
	}

	if (pack_list->num == 0) {
		stringlist_free(pack_list);
		return NULL;
	}

	packed = pack_list->strings[0];
	if (strncmp(packed, POLICY_PACK_PREFIX, POLICY_PACK_OFFSET) != 0) {
		stringlist_free(pack_list);
		return NULL;
	}

	if (unpack(packed, POLICY_PACK_PREFIX, POLICY_PACK_FORMAT, &pol_name) != 0) {

		stringlist_free(pack_list);
		return NULL;
	}

	pol = policy_new(pol_name);
	if (!pol) {
		return NULL;
	}
	free(pol_name); /* policy_new strdup's this */

	for (i = 1; i < pack_list->num; i++) {
		packed = pack_list->strings[i];
		r = resource_unpack(packed);
		if (!r) {
			DEBUG("Unable to unpack: %s", pack);
			stringlist_free(pack_list);
			return NULL;
		}
		policy_add_resource(pol, r);
	}

	stringlist_free(pack_list);
	return pol;
}

