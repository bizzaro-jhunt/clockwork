#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "policy.h"
#include "pack.h"
#include "mem.h"


#define POLICY_PACK_PREFIX "policy::"
#define POLICY_PACK_OFFSET 8
/* Pack format for policy structure (first line):
     a - name
     L - version
 */
#define POLICY_PACK_FORMAT "aL"

enum restype {
	RES_UNKNOWN = 0,
	RES_USER,
	RES_FILE,
	RES_GROUP
};

struct policy_generator {
	struct policy *policy;
	struct hash   *facts;
	enum restype   type;

	union {
		struct res_user  *user;
		struct res_group *group;
		struct res_file  *file;
	} resource;
};

/* Allocate and initialize a new manifest structure */
struct manifest* manifest_new(void)
{
	struct manifest *m;

	m = malloc(sizeof(struct manifest));
	if (m) {
		m->policies = hash_new();
		m->hosts    = hash_new();

		m->nodes = NULL;
		m->nodes_len = 0;

		m->root = manifest_new_stree(m, PROG, NULL, NULL);
	}

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

	stree = calloc(1, sizeof(struct stree));
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

struct hash* fact_read(FILE *io)
{
	assert(io);

	struct hash *facts;
	char buf[8192] = {0};
	char *k, *v;

	facts = hash_new();
	if (!facts) {
		return NULL;
	}

	while (!feof(io)) {
		errno = 0;
		if (!fgets(buf, 8192, io)) {
			if (errno != 0) {
				hash_free(facts);
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

static void * _policy_find_resource(struct policy_generator *pgen, const char *type, const char *id)
{
	assert(pgen);

	char *key;
	void *resource = NULL;

	key = resource_key(type, id);
	if (key) {
		resource = hash_get(pgen->policy->resources, key);
		free(key);
	}

	return resource;
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
		if (strcmp(node->data1, "user") == 0) {
			pgen->type = RES_USER;
			pgen->resource.user = _policy_find_resource(pgen, "res_user", node->data2);
			if (!pgen->resource.user) {
				pgen->resource.user = res_user_new(node->data2);
				policy_add_user_resource(pgen->policy, pgen->resource.user);
			}

		} else if (strcmp(node->data1, "file") == 0) {
			pgen->type = RES_FILE;
			pgen->resource.file = _policy_find_resource(pgen, "res_file", node->data2);
			if (!pgen->resource.file) {
				pgen->resource.file = res_file_new(node->data2);
				policy_add_file_resource(pgen->policy, pgen->resource.file);
			}

		} else if (strcmp(node->data1, "group") == 0) {
			pgen->type = RES_GROUP;
			pgen->resource.group = _policy_find_resource(pgen, "res_group", node->data2);
			if (!pgen->resource.group) {
				pgen->resource.group = res_group_new(node->data2);
				policy_add_group_resource(pgen->policy, pgen->resource.group);
			}

		} else {
			pgen->type = RES_UNKNOWN;
			fprintf(stderr, "error: trying to define unknown resource type '%s'\n", node->data1);
			/* FIXME: asked to define an unknown resource */
		}

		break;

	case ATTR:
		switch (pgen->type) {
		case RES_USER:
			res_user_setattr(pgen->resource.user, node->data1, node->data2);
			break;

		case RES_FILE:
			res_file_setattr(pgen->resource.file, node->data1, node->data2);
			break;

		case RES_GROUP:
			res_group_setattr(pgen->resource.group, node->data1, node->data2);
			break;

		default:
			fprintf(stderr, "error: trying to set attribute %s = '%s' on unknown type %u\n",
					node->data1, node->data2, pgen->type);
		}
		break;

	case PROG:
	case NOOP:
		/* do nothing */
		break;

	default:
		/* FIXME: unexpected node type */
		fprintf(stderr, "error: unexpected node in syntax tree: (%u %s/%s)\n", node->op, node->data1, node->data2);
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
	pgen.policy = policy_new(root->data1, policy_latest_version());

	if (_policy_generate(root, &pgen) != 0) {
		policy_free(pgen.policy);
		return NULL;
	}

	return pgen.policy;
}

struct policy* policy_new(const char *name, uint32_t version)
{
	struct policy *pol;

	pol = calloc(1, sizeof(struct policy));
	if (!pol) {
		return NULL;
	}

	pol->name = xstrdup(name);
	pol->version = version;

	list_init(&pol->res_files);
	list_init(&pol->res_groups);
	list_init(&pol->res_users);

	pol->resources = hash_new();

	return pol;
}

void policy_free(struct policy *pol)
{
	if (pol) {
		hash_free(pol->resources);
		free(pol->name);
	}
	free(pol);
}

void policy_free_all(struct policy *pol)
{
	struct res_user  *ru, *ru_tmp;
	struct res_group *rg, *rg_tmp;
	struct res_file  *rf, *rf_tmp;

	for_each_node_safe(ru, ru_tmp, &pol->res_users,  res) { res_user_free(ru);  }
	for_each_node_safe(rg, rg_tmp, &pol->res_groups, res) { res_group_free(rg); }
	for_each_node_safe(rf, rf_tmp, &pol->res_files,  res) { res_file_free(rf);  }
	policy_free(pol);
}

uint32_t policy_latest_version(void)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) != 0) {
		return 0; /* "invalid" current version */
	}

	return (uint32_t)(tv.tv_sec);
}


int policy_add_file_resource(struct policy *pol, struct res_file *rf)
{
	assert(pol);
	assert(rf);

	list_add_tail(&rf->res, &pol->res_files);
	hash_set(pol->resources, rf->key, rf);
	return 0;
}

int policy_add_group_resource(struct policy *pol, struct res_group *rg)
{
	assert(pol);
	assert(rg);

	list_add_tail(&rg->res, &pol->res_groups);
	hash_set(pol->resources, rg->key, rg);
	return 0;
}

int policy_add_user_resource(struct policy *pol, struct res_user *ru)
{
	assert(pol);
	assert(ru);

	list_add_tail(&ru->res, &pol->res_users);
	hash_set(pol->resources, ru->key, ru);
	return 0;
}

char* policy_pack(struct policy *pol)
{
	assert(pol);

	char *packed = NULL;
	stringlist *pack_list;
	size_t pack_len;

	struct res_user  *ru;
	struct res_group *rg;
	struct res_file  *rf;

	pack_list = stringlist_new(NULL);
	if (!pack_list) {
		return NULL;
	}

	pack_len = pack(NULL, 0, POLICY_PACK_FORMAT, pol->name, pol->version);

	packed = malloc(pack_len + POLICY_PACK_OFFSET);
	strncpy(packed, POLICY_PACK_PREFIX, POLICY_PACK_OFFSET);

	pack(packed + POLICY_PACK_OFFSET, pack_len, POLICY_PACK_FORMAT, pol->name, pol->version);
	if (stringlist_add(pack_list, packed) != 0) {
		goto policy_pack_failed;
	}
	xfree(packed);

	/* pack res_user objects */
	for_each_node(ru, &pol->res_users, res) {
		packed = res_user_pack(ru);
		if (!packed || stringlist_add(pack_list, packed) != 0) {
			goto policy_pack_failed;
		}
		xfree(packed);
	}

	/* pack res_group objects */
	for_each_node(rg, &pol->res_groups, res) {
		packed = res_group_pack(rg);
		if (!packed || stringlist_add(pack_list, packed) != 0) {
			goto policy_pack_failed;
		}
		xfree(packed);
	}

	/* pack res_file objects */
	for_each_node(rf, &pol->res_files, res) {
		packed = res_file_pack(rf);
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
	uint32_t pol_version;

	struct res_user *ru;
	struct res_group *rg;
	struct res_file *rf;

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

	if (unpack(packed + POLICY_PACK_OFFSET, POLICY_PACK_FORMAT,
		&pol_name, &pol_version) != 0) {

		stringlist_free(pack_list);
		return NULL;
	}

	pol = policy_new(pol_name, pol_version);
	if (!pol) {
		return NULL;
	}
	free(pol_name); /* policy_new strdup's this */

	for (i = 1; i < pack_list->num; i++) {
		packed = pack_list->strings[i];
		if (res_user_is_pack(packed) == 0) {
			ru = res_user_unpack(packed);
			if (!ru) {
				return NULL;
			}
			policy_add_user_resource(pol, ru);

		} else if (res_group_is_pack(packed) == 0) {
			rg = res_group_unpack(packed);
			if (!rg) {
				return NULL;
			}
			policy_add_group_resource(pol, rg);

		} else if (res_file_is_pack(packed) == 0) {
			rf = res_file_unpack(packed);
			if (!rf) {
				return NULL;
			}
			policy_add_file_resource(pol, rf);

		} else {
			stringlist_free(pack_list);
			return NULL; /* unknown policy object type! */
		}
	}

	stringlist_free(pack_list);
	return pol;
}

