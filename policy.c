#include <assert.h>
#include <string.h>

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

