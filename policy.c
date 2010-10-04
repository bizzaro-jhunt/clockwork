#include <assert.h>

#include "policy.h"
#include "mem.h"

void policy_init(struct policy *pol, const char *name)
{
	assert(pol);

	pol->name = xstrdup(name);

	list_init(&pol->res_files);
	list_init(&pol->res_groups);
	list_init(&pol->res_users);
}

void policy_free(struct policy *pol)
{
	xfree(pol->name);
}

int policy_add_file_resource(struct policy *pol, struct res_file *rf)
{
	assert(pol);
	assert(rf);

	list_add_tail(&pol->res_files, &rf->res);
	return 0;
}

int policy_add_group_resource(struct policy *pol, struct res_group *rg)
{
	assert(pol);
	assert(rg);

	list_add_tail(&pol->res_groups, &rg->res);
	return 0;
}

int policy_add_user_resource(struct policy *pol, struct res_user *ru)
{
	assert(pol);
	assert(ru);

	list_add_tail(&pol->res_users, &ru->res);
	return 0;
}

