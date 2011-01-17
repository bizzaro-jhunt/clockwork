#include <assert.h>
#include <string.h>
#include <errno.h>

#include "resource.h"
#include "res_group.h"
#include "pack.h"
#include "mem.h"


#define RES_GROUP_PACK_PREFIX "res_group::"
#define RES_GROUP_PACK_OFFSET 11
/* Pack format for res_group structure:
     L - rg_enf
     a - rg_name
     a - rg_passwd
     L - rg_gid
     a - rg_mem_add (joined stringlist)
     a - rg_mem_rm  (joined stringlist)
     a - rg_adm_add (joined stringlist)
     a - rg_adm_rm  (joined stringlist)
 */
#define RES_GROUP_PACK_FORMAT "LaaLaaaa"


static int _res_group_diff(struct res_group*);
static int _group_update(stringlist*, stringlist*, const char*);

/*****************************************************************/

static int _res_group_diff(struct res_group *rg)
{
	assert(rg);
	assert(rg->rg_grp);
	assert(rg->rg_sg);

	stringlist *list; /* for diff'ing gr_mem and sg_adm */

	rg->rg_diff = RES_GROUP_NONE;

	if (res_group_enforced(rg, NAME) && strcmp(rg->rg_name, rg->rg_grp->gr_name) != 0) {
		rg->rg_diff |= RES_GROUP_NAME;
	}

	if (res_group_enforced(rg, PASSWD) && strcmp(rg->rg_passwd, rg->rg_sg->sg_passwd) != 0) {
		rg->rg_diff |= RES_GROUP_PASSWD;
	}

	if (res_group_enforced(rg, GID) && rg->rg_gid != rg->rg_grp->gr_gid) {
		rg->rg_diff |= RES_GROUP_GID;
	}

	if (res_group_enforced(rg, MEMBERS)) {
		/* use list as a stringlist of gr_mem */
		list = stringlist_new(rg->rg_grp->gr_mem);
		if (stringlist_diff(list, rg->rg_mem) == 0) {
			rg->rg_diff |= RES_GROUP_MEMBERS;
		}
		stringlist_free(list);
	}

	if (res_group_enforced(rg, ADMINS)) {
		/* use list as a stringlist of sg_adm */
		list = stringlist_new(rg->rg_sg->sg_adm);
		if (stringlist_diff(list, rg->rg_adm) == 0) {
			rg->rg_diff |= RES_GROUP_ADMINS;
		}
		stringlist_free(list);
	}

	return 0;
}

static int _group_update(stringlist *add, stringlist *rm, const char *user)
{
	/* put user in add */
	if (stringlist_search(add, user) != 0) {
		stringlist_add(add, user);
	}

	/* take user out of rm */
	if (stringlist_search(rm, user) == 0) {
		stringlist_remove(rm, user);
	}

	return 0;
}

/*****************************************************************/

struct res_group* res_group_new(const char *key)
{
	struct res_group *rg;

	rg = malloc(sizeof(struct res_group));
	if (!rg) {
		return NULL;
	}

	list_init(&rg->res);

	rg->rg_name = NULL;
	rg->rg_passwd = NULL;
	rg->rg_gid = 0;

	rg->rg_grp = NULL;
	rg->rg_sg  = NULL;

	rg->rg_mem = NULL;
	rg->rg_mem_add = stringlist_new(NULL);
	rg->rg_mem_rm  = stringlist_new(NULL);

	rg->rg_adm = NULL;
	rg->rg_adm_add = stringlist_new(NULL);
	rg->rg_adm_rm  = stringlist_new(NULL);

	rg->rg_enf  = RES_GROUP_NONE;
	rg->rg_diff = RES_GROUP_NONE;

	if (key) {
		rg->key = resource_key("res_group", key);
		res_group_set_name(rg, key);
	} else {
		rg->key = NULL;
	}

	return rg;
}

void res_group_free(struct res_group *rg)
{
	if (rg) {
		free(rg->key);

		free(rg->rg_name);
		free(rg->rg_passwd);

		list_del(&rg->res);

		if (rg->rg_mem) {
			stringlist_free(rg->rg_mem);
		}
		stringlist_free(rg->rg_mem_add);
		stringlist_free(rg->rg_mem_rm);

		if (rg->rg_adm) {
			stringlist_free(rg->rg_adm);
		}
		stringlist_free(rg->rg_adm_add);
		stringlist_free(rg->rg_adm_rm);
	}
	free(rg);
}

int res_group_setattr(struct res_group *rg, const char *name, const char *value)
{
	if (strcmp(name, "gid") == 0) {
		return res_group_set_gid(rg, strtoll(value, NULL, 10));
	} else if (strcmp(name, "present") == 0) {
		return res_group_set_presence(rg, strcmp(value, "no"));
	} else if (strcmp(name, "member") == 0) {
		return res_group_add_member(rg, value);
	} else if (strcmp(name, "admin") == 0) {
		return res_group_add_admin(rg, value);
	} else if (strcmp(name, "pwhash") == 0 || strcmp(name, "password") == 0) {
		return res_group_set_passwd(rg, value);
	}

	return -1;
}

int res_group_set_presence(struct res_group *rg, int presence)
{
	assert(rg);

	if (presence) {
		rg->rg_enf ^= RES_GROUP_ABSENT;
	} else {
		rg->rg_enf |= RES_GROUP_ABSENT;
	}

	return 0;
}

int res_group_set_name(struct res_group *rg, const char *name)
{
	assert(rg);

	xfree(rg->rg_name);
	rg->rg_name = strdup(name);

	rg->rg_enf |= RES_GROUP_NAME;
	return 0;
}

int res_group_set_passwd(struct res_group *rg, const char *passwd)
{
	assert(rg);

	xfree(rg->rg_passwd);
	rg->rg_passwd = strdup(passwd);
	if (!rg->rg_passwd) { return -1; }

	rg->rg_enf |= RES_GROUP_PASSWD;
	return 0;
}

int res_group_set_gid(struct res_group *rg, gid_t gid)
{
	assert(rg);

	rg->rg_gid = gid;

	rg->rg_enf |= RES_GROUP_GID;
	return 0;
}

int res_group_enforce_members(struct res_group *rg, int enforce)
{
	assert(rg);

	if (enforce) {
		rg->rg_enf |= RES_GROUP_MEMBERS;
	} else {
		rg->rg_enf ^= RES_GROUP_MEMBERS;
	}
	return 0;
}

/* updates rg_mem_add */
int res_group_add_member(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	res_group_enforce_members(rg, 1);
	/* add to rg_mem_add, remove from rg_mem_rm */
	return _group_update(rg->rg_mem_add, rg->rg_mem_rm, user);
}
/* updates rg_mem_rm */
int res_group_remove_member(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	res_group_enforce_members(rg, 1);
	/* add to rg_mem_rm, remove from rg_mem_add */
	return _group_update(rg->rg_mem_rm, rg->rg_mem_add, user);
}

int res_group_enforce_admins(struct res_group *rg, int enforce)
{
	assert(rg);

	if (enforce) {
		rg->rg_enf |= RES_GROUP_ADMINS;
	} else {
		rg->rg_enf ^= RES_GROUP_ADMINS;
	}
	return 0;
}

/* updates rg_adm_add */
int res_group_add_admin(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	res_group_enforce_admins(rg, 1);
	/* add to rg_adm_add, remove from rg_adm_rm */
	return _group_update(rg->rg_adm_add, rg->rg_adm_rm, user);
}

/* updates rg_adm_rm */
int res_group_remove_admin(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	res_group_enforce_admins(rg, 1);
	/* add to rg_adm_rm, remove from rg_adm_add */
	return _group_update(rg->rg_adm_rm, rg->rg_adm_add, user);
}

int res_group_stat(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb)
{
	assert(rg);
	assert(grdb);
	assert(sgdb);

	rg->rg_grp = grdb_get_by_name(grdb, rg->rg_name);
	rg->rg_sg = sgdb_get_by_name(sgdb, rg->rg_name);
	if (!rg->rg_grp || !rg->rg_sg) { /* new group */
		rg->rg_diff = rg->rg_enf;

		rg->rg_mem = stringlist_new(NULL);
		stringlist_add_all(rg->rg_mem, rg->rg_mem_add);

		rg->rg_adm = stringlist_new(NULL);
		stringlist_add_all(rg->rg_adm, rg->rg_adm_add);

		return 0;
	}

	/* set up rg_mem as the list we want for gr_mem */
	rg->rg_mem = stringlist_new(rg->rg_grp->gr_mem);
	stringlist_add_all(rg->rg_mem, rg->rg_mem_add);
	stringlist_remove_all(rg->rg_mem, rg->rg_mem_rm);
	stringlist_uniq(rg->rg_mem);

	/* set up rg_adm as the list we want for sg_adm */
	rg->rg_adm = stringlist_new(rg->rg_sg->sg_adm);
	stringlist_add_all(rg->rg_adm, rg->rg_adm_add);
	stringlist_remove_all(rg->rg_adm, rg->rg_adm_rm);
	stringlist_uniq(rg->rg_adm);

	return _res_group_diff(rg);
}

struct report* res_group_remediate(struct res_group *rg, int dryrun, struct grdb *grdb, struct sgdb *sgdb)
{
	assert(rg);
	assert(grdb);
	assert(sgdb);

	struct report *report;
	char *action;

	int new_group;

	report = report_new("Group", rg->rg_name);

	/* Remove the group if RES_GROUP_ABSENT */
	if (res_group_enforced(rg, ABSENT)) {
		if (rg->rg_grp || rg->rg_sg) {
			action = string("remove group");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else {
				if ((rg->rg_grp && grdb_rm(grdb, rg->rg_grp) != 0)
				 || (rg->rg_sg && sgdb_rm(sgdb, rg->rg_sg) != 0)) {
					report_action(report, action, ACTION_FAILED);
				} else {
					report_action(report, action, ACTION_SUCCEEDED);
				}
			}
		}

		return report;
	}

	if (!rg->rg_grp || !rg->rg_sg) {
		new_group = 1;
		action = string("create group");

		if (!rg->rg_grp) { rg->rg_grp = grdb_new_entry(grdb, rg->rg_name); }
		if (!rg->rg_sg)  { rg->rg_sg = sgdb_new_entry(sgdb, rg->rg_name); }

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (rg->rg_grp && rg->rg_sg) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (res_group_different(rg, PASSWD)) {
		if (new_group) {
			action = string("set group membership password");
		} else {
			action = string("change group membership password");
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			xfree(rg->rg_grp->gr_passwd);
			rg->rg_grp->gr_passwd = strdup("x");
			xfree(rg->rg_sg->sg_passwd);
			rg->rg_sg->sg_passwd = strdup(rg->rg_passwd);

			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_group_different(rg, GID)) {
		if (new_group) {
			action = string("set gid to %u", rg->rg_gid);
		} else {
			action = string("change gid from %u to %u", rg->rg_grp->gr_gid, rg->rg_gid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			rg->rg_grp->gr_gid = rg->rg_gid;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_group_enforced(rg, MEMBERS) && res_group_different(rg, MEMBERS)) {
		if (!dryrun) {
			/* replace gr_mem and sg_mem with the rg_mem stringlist */
			xarrfree(rg->rg_grp->gr_mem);
			rg->rg_grp->gr_mem = xarrdup(rg->rg_mem->strings);

			xarrfree(rg->rg_sg->sg_mem);
			rg->rg_sg->sg_mem = xarrdup(rg->rg_mem->strings);
		}

		size_t i;
		for (i = 0; i < rg->rg_mem_add->num; i++) {
			report_action(report, string("add %s", rg->rg_mem_add->strings[i]), (dryrun ? ACTION_SKIPPED : ACTION_SUCCEEDED));
		}
		for (i = 0; i < rg->rg_mem_rm->num; i++) {
			report_action(report, string("remove %s", rg->rg_mem_rm->strings[i]), (dryrun ? ACTION_SKIPPED : ACTION_SUCCEEDED));
		}
	}

	if (res_group_enforced(rg, ADMINS) && res_group_different(rg, ADMINS)) {
		if (!dryrun) {
			/* replace sg_adm with the rg_adm stringlist */
			xarrfree(rg->rg_sg->sg_adm);
			rg->rg_sg->sg_adm = xarrdup(rg->rg_adm->strings);
		}

		size_t i;
		for (i = 0; i < rg->rg_adm_add->num; i++) {
			report_action(report, string("grant admin rights to %s", rg->rg_adm_add->strings[i]), (dryrun ? ACTION_SKIPPED : ACTION_SUCCEEDED));
		}
		for (i = 0; i < rg->rg_adm_rm->num; i++) {
			report_action(report, string("revoke admin rights from %s", rg->rg_adm_rm->strings[i]), (dryrun ? ACTION_SKIPPED : ACTION_SUCCEEDED));
		}
	}

	return report;
}

int res_group_is_pack(const char *packed)
{
	return strncmp(packed, RES_GROUP_PACK_PREFIX, RES_GROUP_PACK_OFFSET);
}

char *res_group_pack(struct res_group *rg)
{
	char *packed;
	size_t pack_len;
	char *mem_add = NULL, *mem_rm = NULL,
	     *adm_add = NULL, *adm_rm = NULL;

	if (!(mem_add = stringlist_join(rg->rg_mem_add, "."))
	 || !(mem_rm  = stringlist_join(rg->rg_mem_rm,  "."))
	 || !(adm_add = stringlist_join(rg->rg_adm_add, "."))
	 || !(adm_rm  = stringlist_join(rg->rg_adm_rm,  "."))) {
		free(mem_add); free(mem_rm);
		free(adm_add); free(adm_rm);
		return NULL;
	}

	pack_len = pack(NULL, 0, RES_GROUP_PACK_FORMAT,
		rg->rg_enf,
		rg->rg_name, rg->rg_passwd, rg->rg_gid,
		mem_add, mem_rm, adm_add, adm_rm);

	packed = malloc(pack_len + RES_GROUP_PACK_OFFSET);
	strncpy(packed, RES_GROUP_PACK_PREFIX, RES_GROUP_PACK_OFFSET);

	pack(packed + RES_GROUP_PACK_OFFSET, pack_len, RES_GROUP_PACK_FORMAT,
		rg->rg_enf,
		rg->rg_name, rg->rg_passwd, rg->rg_gid,
		mem_add, mem_rm, adm_add, adm_rm);

	free(mem_add); free(mem_rm);
	free(adm_add); free(adm_rm);

	return packed;
}

struct res_group* res_group_unpack(const char *packed)
{
	struct res_group *rg;
	char *mem_add = NULL, *mem_rm = NULL,
	     *adm_add = NULL, *adm_rm = NULL;

	if (strncmp(packed, RES_GROUP_PACK_PREFIX, RES_GROUP_PACK_OFFSET) != 0) {
		return NULL;
	}

	rg = res_group_new(NULL);
	if (unpack(packed + RES_GROUP_PACK_OFFSET, RES_GROUP_PACK_FORMAT,
		&rg->rg_enf,
		&rg->rg_name, &rg->rg_passwd, &rg->rg_gid,
		&mem_add, &mem_rm, &adm_add, &adm_rm)) {

		res_group_free(rg);
		return NULL;
	}

	rg->key = resource_key("res_group", rg->rg_name);

	stringlist_free(rg->rg_mem_add);
	rg->rg_mem_add = stringlist_split(mem_add, strlen(mem_add), ".");

	stringlist_free(rg->rg_mem_rm);
	rg->rg_mem_rm  = stringlist_split(mem_rm,  strlen(mem_rm),  ".");

	stringlist_free(rg->rg_adm_add);
	rg->rg_adm_add = stringlist_split(adm_add, strlen(adm_add), ".");

	stringlist_free(rg->rg_adm_rm);
	rg->rg_adm_rm  = stringlist_split(adm_rm,  strlen(adm_rm),  ".");

	free(mem_add); free(mem_rm);
	free(adm_add); free(adm_rm);

	return rg;
}
