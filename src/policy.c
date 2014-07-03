/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <glob.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "policy.h"
#include "resource.h"

struct scope {
	int depth;
	cw_list_t l;
	cw_list_t res_defs;
};

struct policy_generator {
	struct policy *policy;
	cw_hash_t     *facts;
	enum restype   type;

	cw_list_t scopes;
	struct scope *scope;

	struct resource *res;
	struct dependency *dep;
};

static struct scope* push_scope(cw_list_t *list, int depth)
{
	struct scope *scope = cw_alloc(sizeof(struct scope));

	cw_list_init(&scope->res_defs);
	scope->depth = depth;

	cw_list_unshift(list, &scope->l);
	return scope;
}

static struct scope* pop_scope(cw_list_t *list)
{
	struct scope *scope;

	scope = cw_list_object(list->next, struct scope, l);
	if (!scope) {
		return NULL;
	}
	cw_list_delete(&scope->l);

	struct resource *res_def, *tmp;
	for_each_object_safe(res_def, tmp, &scope->res_defs, l) {
		resource_free(res_def);
	}
	free(scope);

	if (cw_list_isempty(list)) {
		return NULL;
	}
	return cw_list_object(list->next, struct scope, l);
}

static void stree_free(struct stree *n)
{
	if (n) {
		free(n->data1);
		free(n->data2);
		free(n->nodes);
	}
	free(n);
}

static int _policy_normalize(struct policy *pol, cw_hash_t *facts)
{
	assert(pol); // LCOV_EXCL_LINE

	LIST(deps);
	struct resource *r1, *r2, *tmp;
	struct dependency *dep;

	for_each_resource(r1, pol) {
		cw_log(LOG_DEBUG, "Normalizing resource %s", r1->key);
		if (resource_norm(r1, pol, facts) != 0) {
			cw_log(LOG_ERR, "Failed to normalize resource %s", r1->key);
		}
	}

	/* expand defered dependencies */
	for_each_dependency(dep, pol) {
		cw_log(LOG_DEBUG, "Expanding dependency for %s on %s", dep->a, dep->b);

		dep->resource_a = r1 = cw_hash_get(pol->index, dep->a);
		dep->resource_b = r2 = cw_hash_get(pol->index, dep->b);

		if (!r1) {
			cw_log(LOG_ERR, "Failed dependency for unknown resource %s", dep->a);
			return -1;
		}
		if (!r2) {
			cw_log(LOG_ERR, "Failed dependency on unknown resource %s", dep->b);
			return -2;
		}

		resource_add_dependency(r1, r2);
	}

	/* get the one with no deps */
	for_each_resource_safe(r1, tmp, pol) {
		if (r1->ndeps == 0) {
			cw_log(LOG_DEBUG, "%s has no deps // appending", r1->key);
			cw_list_delete(&r1->l);
			cw_list_push(&deps, &r1->l);
		}
	}

	for_each_object(r1, &deps, l) {
		for_each_resource_safe(r2, tmp, pol) {
			if (resource_depends_on(r2, r1) == 0) {
				resource_drop_dependency(r2, r1);
				if (r2->ndeps == 0) {
					cw_log(LOG_DEBUG, "%s has no _outstanding_ deps // appending", r2->key);
					cw_list_delete(&r2->l);
					cw_list_push(&deps, &r2->l);
				}
			}
		}
	}

	if (!cw_list_isempty(&pol->resources)) {
		cw_log(LOG_ERR, "Leftover resources after dependency resolution");
		return -3;
	}
	cw_list_replace(&deps, &pol->resources);

	return 0;
}

/**
  Create a new manifest.

  **Note:** The pointer returned must be freed via @manifest_free.

  On success, returns a new manifest structure.  On failure, returns NULL.
 */
struct manifest* manifest_new(void)
{
	struct manifest *m;

	m = cw_alloc(sizeof(struct manifest));
	m->policies = cw_alloc(sizeof(cw_hash_t));
	m->hosts    = cw_alloc(sizeof(cw_hash_t));

	m->nodes = NULL;
	m->nodes_len = 0;

	m->root = manifest_new_stree(m, PROG, NULL, NULL);

	return m;
}

/**
  Free manifest $m.
 */
void manifest_free(struct manifest *m)
{
	size_t i;

	if (m) {
		for (i = 0; i < m->nodes_len; i++) { stree_free(m->nodes[i]); }
		free(m->nodes);

		cw_hash_done(m->policies, 0);
		cw_hash_done(m->hosts,    0);

		free(m->policies);
		free(m->hosts);
	}
	free(m);
}

static int _manifest_validate(struct stree *node)
{
	if (!node) return 0;
	int i;

	cw_log(LOG_DEBUG, "node: %u %s/%s)", node->op, node->data1, node->data2);

	switch (node->op) {
	case RESOURCE:
		if (resource_type(node->data1) == RES_UNKNOWN) {
			cw_log(LOG_ERR, "Unknown resource type '%s'", node->data1);
			return 1;
		}

	case IF:
	case PROG:
	case NOOP:
	case HOST:
	case POLICY:
	case INCLUDE:
	case DEPENDENCY:
	case ATTR:
	case RESOURCE_ID:
		for (i = 0; i < node->size; i++)
			if (_manifest_validate(node->nodes[i]) != 0)
				return 1;
		return 0;

	default:
		cw_log(LOG_ERR, "unexpected node in syntax tree: %u %s/%s)", node->op, node->data1, node->data2);
		return 1;
	}

	return 0;
}

/**
  Validate resource types in a manifest
 */
int manifest_validate(struct manifest *m)
{
	struct stree *node;
	if (_manifest_validate(node) != 0) return 1;

	/* walk all the hosts */
	char *host;
	for_each_key_value(m->hosts, host, node)
		if (_manifest_validate(node) != 0) return 1;

	/* walk all the policies */
	char *policy;
	for_each_key_value(m->policies, policy, node)
		if (_manifest_validate(node) != 0) return 1;

	return 0;
}

/**
  Create a new syntax tree node for $m.

  $op represents the type of node.

  This function not only allocates and initializes an stree node,
  it also stores a reference to the allocate stree node, so that
  manifest_free can properly de-allocate the node.  For this reason,
  it is not advised to allocate stree nodes for use in a manifest
  through any other mechanism.

  $data1 and $data2 are operation-specific.

  **Note:** for reasons of optimization, $data1 and $data2 are
  "taken over" by this function.  *Do not free them yourself*;
  you risk a double-free memory corruption.

  On success, returns a new syntax tree node that has been added
  to $m already.  Operation-specific data will be set to $data1
  and $data2 for you.

  On failure, returns NULL.
 */
struct stree* manifest_new_stree(struct manifest *m, enum oper op, char *data1, char *data2)
{
	struct stree *stree;
	struct stree **list;

	stree = cw_alloc(sizeof(struct stree));
	if (!stree) {
		return NULL;
	}
	list = realloc(m->nodes, sizeof(struct stree*) * (m->nodes_len + 1));
	if (!list) {
		free(stree);
		free(m->nodes);
		m->nodes = NULL;
		return NULL;
	}

	stree->op = op;
	stree->data1 = data1;
	stree->data2 = data2;

	list[m->nodes_len++] = stree;
	m->nodes = list;

	return stree;
}

/**
  Add one $child to $parent.

  This function only adds references, it does not duplicate $child
  or $parent.

  **Note:** Both $parent and $child should already be referenced by
  a manifest; they should have been created by a call to
  @manifest_new_stree.

  On success, returns 0.  On failure, returns non-zero.
 */
int stree_add(struct stree *parent, struct stree *child)
{
	assert(parent); // LCOV_EXCL_LINE

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

/*
  Recursively compare $a and $b for equivalence.

  If either $a or $b is NULL, this function will always return non-zero.

  If $a does not contain the same node layout (i.e. parentage, number of
  children, etc.) as $b, then the two trees are considered non-equivalent,
  and this function will return non-zero.

  When compareing trees, the following fields are considered:

  - $op - Integer Comparison
  - $data1 - String comparison (or NULL equivalence)
  - $data2 - String comparison (or NULL equivalence)
  - $nodes - Recursive call to `stree_compare` for each.

  If $a == $b, returns 0.  Otherwise, returns non-zero.
 */
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

int fact_parse(const char *line, cw_hash_t *h)
{
	assert(line); // LCOV_EXCL_LINE
	assert(h); // LCOV_EXCL_LINE

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

	cw_hash_set(h, name, cw_strdup(value));
	free(buf);
	return 0;
}

int fact_exec_read(const char *script, cw_hash_t *facts)
{
	pid_t pid;
	int pipefd[2], rc;
	FILE *input;
	char *path_copy, *arg0;

	cw_log(LOG_INFO, "Processing script %s", script);

	if (pipe(pipefd) != 0) {
		perror("gather_facts");
		return -1;
	}

	pid = fork();
	switch (pid) {
	case -1:
		perror("gather_facts: fork");
		return -1;

	case 0: /* in child */
		close(pipefd[0]);
		close(0); close(1); close(2);

		dup2(pipefd[1], 1); /* dup pipe as stdout */

		path_copy = strdup(script);
		arg0 = basename(path_copy);

		execl(script, arg0, NULL);
		exit(127); /* if execl returns, we failed */

	default: /* in parent */
		close(pipefd[1]);
		input = fdopen(pipefd[0], "r");

		fact_read(input, facts);
		waitpid(pid, &rc, 0);
		fclose(input);
		close(pipefd[0]);

		// treat a fail to exec as an error;
		// handle everything else (including signalled,
		//  non-zero exit, core dumps and crashs)
		return (WIFEXITED(rc) && WEXITSTATUS(rc) == 127) ? -1 : 0;
	}
}

int fact_cat_read(const char *file, cw_hash_t *facts)
{
	assert(file); // LCOV_EXCL_LINE
	assert(facts); // LCOV_EXCL_LINE

	FILE *input = fopen(file, "r");
	if (!input) {
		perror(file);
		return -1;
	}
	if (fact_read(input, facts) == NULL) {
		return -1;
	}
	return 0;
}

int fact_gather(const char *paths, cw_hash_t *facts)
{
	glob_t scripts;
	size_t i;

	switch(glob(paths, GLOB_MARK, NULL, &scripts)) {
	case GLOB_NOMATCH:
		globfree(&scripts);
		if (fact_exec_read(paths, facts) != 0) {
			cw_hash_done(facts, 0);
			return -1;
		}
		return 0;

	case GLOB_NOSPACE:
	case GLOB_ABORTED:
		cw_hash_done(facts, 0);
		return -1;

	}

	for (i = 0; i < scripts.gl_pathc; i++) {
		fact_exec_read(scripts.gl_pathv[i], facts);
	}

	globfree(&scripts);
	return 0;
}


/**
  Read $facts from $io.

  Facts are read in from $io in key=value\n form.  As each fact
  is read, it will be stored in $facts.  Parsing stops when $io
  reaches EOF.

  **Note:** the $facts value-result parameter is not cleared by
  `fact_read.`  In this way, callers can read a body of facts from
  multiple files or IO sources.

  On success, returns $facts.  On failure, returns NULL, and the
  contents of $facts is undefined.
 */
cw_hash_t* fact_read(FILE *io, cw_hash_t *facts)
{
	assert(io); // LCOV_EXCL_LINE

	char buf[8192] = {0};
	int allocated = 0;

	if (!facts) {
		facts = cw_alloc(sizeof(cw_hash_t));
		allocated = 1;
	}

	if (!facts) {
		return NULL;
	}

	while (!feof(io)) {
		errno = 0;
		if (!fgets(buf, 8192, io)) {
			if (errno != 0) {
				if (allocated) cw_hash_done(facts, 0);
				facts = NULL;
			}
			break;
		}
		fact_parse(buf, facts);
	}

	return facts;
}

cw_hash_t* fact_read_string(const char *s, cw_hash_t *facts)
{
	assert(s); // LCOV_EXCL_LINE
	assert(facts); // LCOV_EXCL_LINE

	size_t i;
	struct stringlist *lines = stringlist_split(s, strlen(s), "\n", SPLIT_GREEDY);
	if (!lines) return NULL;

	for (i = 0; i < lines->num; i++) {
		fact_parse(lines->strings[i], facts);
	}

	stringlist_free(lines);
	return facts;
}

/**
  Write $facts to $io.

  Facts are written to $io in key=value\\n format.  They
  are read in order, sorted lexically by key.

  On success, returns 0.  On failure, returns non-zero.
 */
int fact_write(FILE *io, cw_hash_t *facts)
{
	assert(io); // LCOV_EXCL_LINE
	assert(facts); // LCOV_EXCL_LINE

	struct stringlist *lines;
	char buf[8192] = {0};
	char *k; void *v;
	size_t i;

	lines = stringlist_new(NULL);
	for_each_key_value(facts, k, v) {
		snprintf(buf, 8192, "%s=%s\n", k, (const char*)v);
		stringlist_add(lines, buf);
	}

	stringlist_sort(lines, STRINGLIST_SORT_ASC);
	for_each_string(lines, i) {
		fputs(lines->strings[i], io);
	}

	stringlist_free(lines);
	return 0;
}

static struct resource * _policy_find_resource(struct policy_generator *pgen, const char *type, const char *id)
{
	assert(pgen); // LCOV_EXCL_LINE

	char *key;
	struct resource *r = NULL;

	if ((key = cw_string("%s:%s", type, id)) != NULL) {
		r = cw_hash_get(pgen->policy->index, key);
		free(key);
	}

	return r;
}

static struct resource * _policy_make_resource(struct policy_generator *pgen, const char *type, const char *name)
{
	struct resource *res;
	if (name == NULL) { /* default resource */
		res = resource_new(type, NULL);
		cw_list_unshift(&pgen->scope->res_defs, &res->l);
		return res;
	}

	res = _policy_find_resource(pgen, type, name);
	if (!res) { /* new resource; check defaults */
		enum restype rtype = resource_type(type);
		struct resource *defaults;
		for_each_object(defaults, &pgen->scope->res_defs, l) {
			if (defaults->type == rtype) {
				res = resource_clone(defaults, name);
				break;
			}
		}
		if (!res) { /* new resource; no defaults defined */
			res = resource_new(type, name);
		}
	}

	return res;
}

static int _policy_generate(struct stree *node, struct policy_generator *pgen, int depth)
{
	assert(node); // LCOV_EXCL_LINE
	assert(pgen); // LCOV_EXCL_LINE

	unsigned int i;
	struct dependency dep;

again:
	switch(node->op) {
	case IF:
		if (cw_strcmp(cw_hash_get(pgen->facts, node->data1), node->data2) == 0) {
			node = node->nodes[0];
		} else {
			node = node->nodes[1];
		}
		goto again;

	case RESOURCE:
		while (pgen->scope && depth <= pgen->scope->depth) {
			pgen->scope = pop_scope(&pgen->scopes);
		}

		if (!pgen->scope) {
			cw_log(LOG_ERR, "Resource %s/%s defined outside of policy!!",
					node->data1, (node->data2 ? node->data2 : "defaults"));
			return -1;
		}

		pgen->res = _policy_make_resource(pgen, node->data1, node->data2);
		if (!pgen->res) {
			cw_log(LOG_WARNING, "Definition for unknown resource type '%s'", node->data1);
		} else if (cw_list_isempty(&pgen->res->l)) {
			policy_add_resource(pgen->policy, pgen->res);
		}

		break;

	case ATTR:
		if (pgen->res) {
			if (resource_set(pgen->res, node->data1, node->data2) != 0) {
				cw_log(LOG_WARNING, "Unknown Attribute %s = '%s'",
					node->data1, node->data2);
			}
		} else {
			cw_log(LOG_WARNING, "Attribute %s = '%s' defined for unknown type",
			        node->data1, node->data2);
		}

		break;

	case DEPENDENCY:
		if (node->size != 2) {
			cw_log(LOG_ERR, "Corrupt dependency: %u constituent(s)", node->size);
			return -1;
		}

		dep.a = cw_string("%s:%s", node->nodes[0]->data1, node->nodes[0]->data2);
		dep.b = cw_string("%s:%s", node->nodes[1]->data1, node->nodes[1]->data2);
		pgen->dep = dependency_new(dep.a, dep.b);
		free(dep.a);
		free(dep.b);

		policy_add_dependency(pgen->policy, pgen->dep);
		return 0; /* don't need to traverse the RESOURCE_ID nodes */

	case PROG:
	case NOOP:
	case HOST:
	case INCLUDE:
		/* do nothing */
		break;

	case POLICY:
		pgen->scope = push_scope(&pgen->scopes, depth);
		break;

	default:
		cw_log(LOG_WARNING, "unexpected node in syntax tree: (%u %s/%s)", node->op, node->data1, node->data2);
		break;

	}

	for (i = 0; i < node->size; i++) {
		if (_policy_generate(node->nodes[i], pgen, depth+1) != 0) {
			return -1;
		}
	}

	return 0;
}

/**
  Apply $facts to a syntax tree to create a policy.

  This function is crucial to Clockwork's master daemon.
  It turns the abstract syntax tree $root into a full policy
  object by evaluating all conditional constructs against $facts.

  **Note:** the policy returned must be freed with @policy_free.

  On success, returns a new policy object.  On failure, returns NULL.
 */
struct policy* policy_generate(struct stree *root, cw_hash_t *facts)
{
	assert(root); // LCOV_EXCL_LINE

	struct policy_generator pgen;

	pgen.facts = facts;
	pgen.policy = policy_new(root->data1);

	/* set up scopes for default values */
	cw_list_init(&pgen.scopes);
	pgen.scope = NULL;

	if (_policy_generate(root, &pgen, 0) != 0) {
		policy_free(pgen.policy);
		return NULL;
	}

	/* pop (and free) and leftover scopes */
	while (pop_scope(&pgen.scopes))
		;

	int rc = _policy_normalize(pgen.policy, facts);
	assert(rc == 0);

	return pgen.policy;
}

/**
  Create a new, empty policy.

  The policy's name will be set to $name.

  **Note:** the policy returned must be freed with @policy_free.

  On success, returns a new policy object.
  On failure, returns NULL.
 */
struct policy* policy_new(const char *name)
{
	struct policy *pol;

	pol = cw_alloc(sizeof(struct policy));
	pol->name = cw_strdup(name);

	cw_list_init(&pol->resources);
	cw_list_init(&pol->dependencies);
	pol->index = cw_alloc(sizeof(cw_hash_t));
	pol->cache = cw_alloc(sizeof(cw_hash_t));

	return pol;
}

/**
  Free policy $pol.

  This function does not free the resources that $pol
  references.  For that behavior, see @policy_free_all.
 */
void policy_free(struct policy *pol)
{
	if (pol) {
		cw_hash_done(pol->index, 0);
		cw_hash_done(pol->cache, 0);
		free(pol->index);
		free(pol->cache);
		free(pol->name);
	}
	free(pol);
}

/**
  Free $pol, and all of its resources.
 */
void policy_free_all(struct policy *pol)
{
	struct resource *r, *r_tmp;
	struct dependency *d, *d_tmp;

	if (pol) {
		for_each_resource_safe(r, r_tmp, pol) { resource_free(r); }
		for_each_dependency_safe(d, d_tmp, pol) { dependency_free(d); }
	}
	policy_free(pol);
}

/**
  Add resource $res to $pol.

  On success, returns 0.  On failure, returns non-zero.
 */
int policy_add_resource(struct policy *pol, struct resource *res)
{
	assert(pol); // LCOV_EXCL_LINE
	assert(res); // LCOV_EXCL_LINE

	cw_list_push(&pol->resources, &res->l);
	cw_log(LOG_DEBUG, "Adding resource %s to policy", res->key);
	cw_hash_set(pol->index, res->key, res);
	return 0;
}

/**
  Find a resource in $pol, through attribute searching.

  This function is used to find resources defined inside of a policy
  based on a single attribute filter.  For example, to find the res_user
  resource of the user account 'bob':

  <code>
  struct resource *bob;
  bob = policy_find_resource(pol, RES_USER, "username", "bob");
  </code>

  Resource normalization, which often involves creating implicit
  dependencies on other resources, makes heavy use of this function.

  **Note:** If multiple resources match, only the first is returned.

  Returns a pointer to the first resource match,
  or NULL if none matched.
 */
struct resource* policy_find_resource(struct policy *pol, enum restype type, const char *attr, const char *value)
{
	struct resource *r;

	cw_log(LOG_DEBUG, "Looking for resource %u matching %s => '%s'", type, attr, value);
	char *needle = cw_string("%i:%s=%s", (int)type, attr, value);
	r = cw_hash_get(pol->cache, needle);
	if (r) {
		cw_log(LOG_DEBUG, "Search satisfied out from cache\n");
		if (r == (struct resource *)1) r = NULL;

	} else {
		for_each_resource(r, pol) {
			if ((r->type == RES_NONE || r->type == type)
			  && resource_match(r, attr, value) == 0) {
				cw_hash_set(pol->cache, needle, r);
				goto done;
			}
		}
		cw_log(LOG_DEBUG, "  none found...");
		cw_hash_set(pol->cache, needle, (struct resource *)1);
		r = NULL;
	}

done:
	free(needle);
	return r;
}

/**
  Add resource dependency $dep to $pol.

  On success, returns 0.  On failure, returns non-zero.
 */
int policy_add_dependency(struct policy *pol, struct dependency *dep)
{
	assert(pol); // LCOV_EXCL_LINE
	assert(dep); // LCOV_EXCL_LINE

	struct dependency *d;
	for_each_dependency(d, pol) {
		if (strcmp(d->a, dep->a) == 0
		 && strcmp(d->b, dep->b) == 0) {
			cw_log(LOG_DEBUG, "Already have a dependency of %s -> %s", dep->a, dep->b);
			return -1; /* duplicate */
		}
	}
	cw_log(LOG_DEBUG, "Adding dependency of %s -> %s", dep->a, dep->b);
	cw_list_push(&pol->dependencies, &dep->l);

	return 0;
}

int policy_gencode(const struct policy *pol, FILE *io)
{
	struct resource *r;
	unsigned int next = 0;
	for_each_resource(r, pol) {
		next++;
		fprintf(io, "RESET\n");
		resource_gencode(r, io, next);
		fprintf(io, "next.%i:\n", next);

		/* notifications */
		fprintf(io, "!FLAGGED? :changed @final.%i\n", next);
		struct dependency *d;
		for_each_dependency(d, pol) {
			if (r == d->resource_b)
				fprintf(io, "  FLAG 1 :res%i\n", d->resource_a->serial);
		}
		fprintf(io, "final.%i:\n", next);
	}
	return 0;
}
