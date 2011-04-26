#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "policy.h"
#include "pack.h"
#include "resource.h"
#include "stringlist.h"

/** @cond false */
struct policy_generator {
	struct policy *policy;
	struct hash   *facts;
	enum restype   type;

	struct resource *res;
	struct dependency *dep;
};

/** @endcond */

static int _policy_normalize(struct policy *pol)
{
	assert(pol);

	LIST(deps);
	struct resource *r1, *r2, *tmp;
	struct dependency *dep;

	for_each_node(r1, &pol->resources, l) {
		if (resource_norm(r1, pol) != 0) { return -1; }
	}

	/* expand defered dependencies */
	for_each_node(dep, &pol->dependencies, l) {
		DEBUG("Expanding dependency for %s on %s", dep->a, dep->b);

		r1 = hash_get(pol->index, dep->a);
		r2 = hash_get(pol->index, dep->b);

		if (!r1) {
			ERROR("Failed dependency for unknown resource %s", dep->a);
			return -1;
		}
		if (!r2) {
			ERROR("Failed dependency on unknown resource %s", dep->b);
			return -1;
		}

		resource_add_dependency(r1, r2);
	}

	/* get the one with no deps */
	for_each_node_safe(r1, tmp, &pol->resources, l) {
		if (r1->ndeps == 0) { list_move_tail(&r1->l, &deps); }
	}

	for_each_node(r1, &deps, l) {
		for_each_node_safe(r2, tmp, &pol->resources, l) {
			if (resource_depends_on(r2, r1) == 0) {
				resource_drop_dependency(r2, r1);
				if (r2->ndeps == 0) {
					list_move_tail(&r2->l, &deps);
				}
			}
		}
	}

	if (!list_empty(&pol->resources)) { return -1; }
	list_replace(&deps, &pol->resources);

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
	struct dependency dep;

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

	case DEPENDENCY:
		if (node->size != 2) {
			WARNING("Corrupt dependency: %u constituent(s)", node->size);
			return -1;
		}

		dep.a = string("res_%s:%s", node->nodes[0]->data1, node->nodes[0]->data2);
		dep.b = string("res_%s:%s", node->nodes[1]->data1, node->nodes[1]->data2);
		pgen->dep = dependency_new(dep.a, dep.b);
		free(dep.a);
		free(dep.b);

		policy_add_dependency(pgen->policy, pgen->dep);
		return 0; /* don't need to traverse the RESOURCE_ID nodes */

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
	list_init(&pol->dependencies);
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
	DEBUG("Adding resource %s to policy", res->key);
	hash_set(pol->index, res->key, res);
	return 0;
}

struct resource* policy_find_resource(struct policy *pol, enum restype type, const char *attr, const char *value)
{
	struct resource *r;

	for_each_node(r, &pol->resources, l) {
		if (r->type == type && resource_match(r, attr, value) == 0) {
			return r;
		}
	}

	return NULL;
}

int policy_add_dependency(struct policy *pol, struct dependency *dep)
{
	assert(pol);
	assert(dep);

	struct dependency *d;
	for_each_node(d, &pol->dependencies, l) {
		if (strcmp(d->a, dep->a) == 0
		 && strcmp(d->b, dep->b) == 0) {
			DEBUG("Already have a dependency of %s -> %s", dep->a, dep->b);
			return -1; /* duplicate */
		}
	}
	DEBUG("Adding dependency of %s -> %s", dep->a, dep->b);
	list_add_tail(&dep->l, &pol->dependencies);

	return 0;
}

int policy_notify(const struct policy *pol, const struct resource *cause)
{
	assert(pol);
	assert(cause);

	struct dependency *d;

	DEBUG("Notifying dependent resources on %s", cause->key);
	for_each_node(d, &pol->dependencies, l) {
		if (d->resource_b == cause) {
			DEBUG("  notifying resource %s (%p) of change in %s (%p)",
			      d->a, d->resource_a, cause->key, cause);

			resource_notify(d->resource_a, cause);
		}
	}

	return 0;
}

char* policy_pack(const struct policy *pol)
{
	assert(pol);

	char *packed = NULL;
	stringlist *pack_list;
	struct resource *r;
	struct dependency *d;

	pack_list = stringlist_new(NULL);
	if (!pack_list) {
		return NULL;
	}

	packed = pack("policy::", "a", pol->name);
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

	for_each_node(d, &pol->dependencies, l) {
		packed = dependency_pack(d);
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
	struct dependency *d;

	pack_list = stringlist_split(packed_policy, strlen(packed_policy), "\n");
	if (!pack_list) {
		return NULL;
	}

	if (pack_list->num == 0) {
		goto policy_unpack_failed;
	}

	packed = pack_list->strings[0];
	if (strncmp(packed, "policy::", strlen("policy::")) != 0) {
		goto policy_unpack_failed;
	}

	if (unpack(packed, "policy::", "a", &pol_name) != 0) {
		goto policy_unpack_failed;
	}

	pol = policy_new(pol_name);
	if (!pol) {
		goto policy_unpack_failed;
	}
	free(pol_name); /* policy_new strdup's this */

	for (i = 1; i < pack_list->num; i++) {
		packed = pack_list->strings[i];
		if (strncmp(packed, "dependency::", 12) == 0) {
			d = dependency_unpack(packed);
			if (!d) {
				DEBUG("Unable to unpack dependency: %s", packed);
				goto policy_unpack_failed;
			}

			d->resource_a = hash_get(pol->index, d->a);
			d->resource_b = hash_get(pol->index, d->b);
			if (!d->resource_a) {
				ERROR("Failed resolving dependency for %s", d->a);
				goto policy_unpack_failed;
			}
			if (!d->resource_b) {
				ERROR("Failed resolving dependency for %s", d->b);
				goto policy_unpack_failed;
			}
			policy_add_dependency(pol, d);

		} else {
			r = resource_unpack(packed);
			if (!r) {
				DEBUG("Unable to unpack: %s", pack);
				goto policy_unpack_failed;
			}
			policy_add_resource(pol, r);

		}
	}

	stringlist_free(pack_list);
	return pol;

policy_unpack_failed:
	stringlist_free(pack_list);
	return NULL;
}

