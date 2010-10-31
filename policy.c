#include <assert.h>
#include <string.h>

#include "policy.h"
#include "mem.h"

struct policy* policy_new(const char *name, uint32_t version)
{
	struct policy *pol;

	pol = calloc(1, sizeof(struct policy));
	if (!pol) {
		return NULL;
	}

	if (policy_init(pol, name, version) != 0) {
		free(pol);
		return NULL;
	}

	return pol;
}

int policy_init(struct policy *pol, const char *name, uint32_t version)
{
	assert(pol);

	pol->name = xstrdup(name);
	pol->version = version;

	list_init(&pol->res_files);
	list_init(&pol->res_groups);
	list_init(&pol->res_users);

	return 0;
}

void policy_deinit(struct policy *pol)
{
	xfree(pol->name);
	pol->version = 0;

	list_init(&pol->res_files);
	list_init(&pol->res_groups);
	list_init(&pol->res_users);
}

void policy_free(struct policy *pol)
{
	policy_deinit(pol);
	free(pol);
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

char* policy_pack(struct policy *pol)
{
	assert(pol);

	char *packed = NULL;
	stringlist *pack_list;

	struct res_user  *ru;
	struct res_group *rg;
	struct res_file  *rf;

	pack_list = stringlist_new(NULL);
	if (!pack_list) {
		return NULL;
	}

	/* serialize res_user objects */
	for_each_node(ru, &pol->res_users, res) {
		packed = res_user_pack(ru);
		if (!packed || stringlist_add(pack_list, packed) != 0) {
			goto policy_pack_failed;
		}
		xfree(packed);
	}

	/* serialize res_group objects */
	for_each_node(rg, &pol->res_groups, res) {
		packed = res_group_pack(rg);
		if (!packed || stringlist_add(pack_list, packed) != 0) {
			goto policy_pack_failed;
		}
		xfree(packed);
	}

	/* serialize res_file objects */
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

	struct res_user *ru;
	struct res_group *rg;
	struct res_file *rf;

	pol = policy_new("FAKE POLICY", 12345); /* FIXME: hard-coding values; need to be in pack policy */
	if (!pol) {
		return NULL;
	}

	pack_list = stringlist_split(packed_policy, strlen(packed_policy), "\n");
	if (!pack_list) {
		return NULL;
	}

	for (i = 0; i < pack_list->num; i++) {
		packed = pack_list->strings[i];
		if (strncmp(packed, "res_user::", 10) == 0) {
			ru = res_user_unpack(packed);
			if (!ru) {
				return NULL;
			}
			policy_add_user_resource(pol, ru);

		} else if (strncmp(packed, "res_group::", 11) == 0) {
			rg = res_group_unpack(packed);
			if (!rg) {
				return NULL;
			}
			policy_add_group_resource(pol, rg);

		} else if (strncmp(packed, "res_file::", 10) == 0) {
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

