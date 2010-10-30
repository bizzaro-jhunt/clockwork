#ifndef _POLICY_H
#define _POLICY_H

#include "list.h"
#include "res_file.h"
#include "res_group.h"
#include "res_user.h"

struct policy {
	char        *name;       /* User-assigned name of policy */
	uint32_t     version;    /* Policy version number */

	/* Components */
	struct list  res_files;
	struct list  res_groups;
	struct list  res_users;
};

void policy_init(struct policy *pol, const char *name, uint32_t version);
void policy_free(struct policy *pol);

int policy_add_file_resource(struct policy *pol, struct res_file *rf);
int policy_add_group_resource(struct policy *pol, struct res_group *rg);
int policy_add_user_resource(struct policy *pol, struct res_user *ru);

int policy_serialize(struct policy *pol, char **dst, size_t *len);
int policy_unserialize(struct policy *pol, char *src, size_t len);

#endif /* _POLICY_H */
