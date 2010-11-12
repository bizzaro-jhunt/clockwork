#ifndef _POLICY_H
#define _POLICY_H

#include "list.h"
#include "res_file.h"
#include "res_group.h"
#include "res_user.h"

enum oper {
	NOOP = 0,
	PROG,
	IF,
	INCLUDE,
	ENFORCE,
	POLICY,
	HOST,
	RESOURCE,
	ATTR
};

struct stree {
	enum oper op;
	char *data1, *data2;

	unsigned int size;
	struct stree **nodes;
};

struct manifest {
	struct host **hosts;
	size_t hosts_len;

	struct stree **nodes;
	size_t nodes_len;

	struct stree *root;
};

/* According to RFC 1034 */
#define HOST_NAME_MAX 256
struct host {
	char name[HOST_NAME_MAX];
	struct stree *policy;
};

struct fact {
	char *name;
	char *value;

	struct list facts;
};


struct policy {
	char        *name;       /* User-assigned name of policy */
	uint32_t     version;    /* Policy version number */

	/* Components */
	struct list  res_files;
	struct list  res_groups;
	struct list  res_users;
};

struct manifest* manifest_new(void);
void manifest_free(struct manifest *m);
struct host*  manifest_new_host(struct manifest *m, const char *name, struct stree *node);
struct stree* manifest_new_stree(struct manifest *m, enum oper op, const char *data1, const char *data2);

int stree_add(struct stree *parent, struct stree *child);
int stree_expand(struct stree *root);
int stree_compare(const struct stree *a, const struct stree *b);

void fact_free_all(struct list *facts);
struct fact *fact_parse(const char *line);
int fact_read(struct list *facts, FILE *io);

struct policy* policy_generate(struct stree *root, struct list *facts);

struct policy* policy_new(const char *name, uint32_t version);
int  policy_init(struct policy *pol, const char *name, uint32_t version);
void policy_deinit(struct policy *pol);
void policy_free(struct policy *pol);
void policy_free_all(struct policy *pol);

uint32_t policy_latest_version(void);

int policy_add_file_resource(struct policy *pol, struct res_file *rf);
int policy_add_group_resource(struct policy *pol, struct res_group *rg);
int policy_add_user_resource(struct policy *pol, struct res_user *ru);

char* policy_pack(struct policy *pol);
struct policy* policy_unpack(const char *packed);

#endif /* _POLICY_H */
