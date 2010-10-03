#include <assert.h>
#include <string.h>
#include <errno.h>

#include "res_group.h"
#include "mem.h"

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

void res_group_init(struct res_group *rg)
{
	rg->rg_prio = 0;

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

	rg->rg_enf = RES_GROUP_NONE;
	rg->rg_diff = RES_GROUP_NONE;
}

void res_group_free(struct res_group *rg)
{
	xfree(rg->rg_name);
	xfree(rg->rg_passwd);

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

int res_group_set_name(struct res_group *rg, const char *name)
{
	assert(rg);

	xfree(rg->rg_name);
	rg->rg_name = strdup(name);

	rg->rg_enf |= RES_GROUP_NAME;
	return 0;
}

int res_group_unset_name(struct res_group *rg)
{
	assert(rg);

	rg->rg_enf ^= RES_GROUP_NAME;
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

int res_group_unset_passwd(struct res_group *rg)
{
	assert(rg);

	rg->rg_enf ^= RES_GROUP_PASSWD;
	return 0;
}

int res_group_set_gid(struct res_group *rg, gid_t gid)
{
	assert(rg);

	rg->rg_gid = gid;

	rg->rg_enf |= RES_GROUP_GID;
	return 0;
}

int res_group_unset_gid(struct res_group *rg)
{
	assert(rg);

	rg->rg_enf ^= RES_GROUP_GID;
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

	/* add to rg_mem_add, remove from rg_mem_rm */
	return _group_update(rg->rg_mem_add, rg->rg_mem_rm, user);
}
/* updates rg_mem_rm */
int res_group_remove_member(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

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

	/* add to rg_adm_add, remove from rg_adm_rm */
	return _group_update(rg->rg_adm_add, rg->rg_adm_rm, user);
}

/* updates rg_adm_rm */
int res_group_remove_admin(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	/* add to rg_adm_rm, remove from rg_adm_add */
	return _group_update(rg->rg_adm_rm, rg->rg_adm_add, user);
}

void res_group_merge(struct res_group *rg1, struct res_group *rg2)
{
	assert(rg1);
	assert(rg2);

	struct res_group *tmp;

	if (rg1->rg_prio > rg2->rg_prio) {
		/* out-of-order, swap pointers */
		tmp = rg1; rg1 = rg2; rg2 = tmp; tmp = NULL;
	}

	/* rg1 has priority over rg2 */
	assert(rg1->rg_prio <= rg2->rg_prio);

	if ( res_group_enforced(rg2, NAME) &&
	    !res_group_enforced(rg1, NAME)) {
		res_group_set_name(rg1, rg2->rg_name);
	}

	if ( res_group_enforced(rg2, PASSWD) &&
	    !res_group_enforced(rg1, PASSWD)) {
		res_group_set_passwd(rg1, rg2->rg_passwd);
	}

	if ( res_group_enforced(rg2, GID) &&
	    !res_group_enforced(rg1, GID)) {
		res_group_set_gid(rg1, rg2->rg_gid);
	}

	if (res_group_enforced(rg2, MEMBERS)) {
		stringlist_add_all(rg1->rg_mem_add, rg2->rg_mem_add);
		stringlist_add_all(rg1->rg_mem_rm,  rg2->rg_mem_rm);
		res_group_enforce_members(rg1, 1);
	}

	if (res_group_enforced(rg2, ADMINS)) {
		stringlist_add_all(rg1->rg_adm_add, rg2->rg_adm_add);
		stringlist_add_all(rg1->rg_adm_rm,  rg2->rg_adm_rm);
		res_group_enforce_admins(rg1, 1);
	}
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

int res_group_remediate(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb)
{
	assert(rg);
	assert(grdb);
	assert(sgdb);

	if (!rg->rg_grp) {
		rg->rg_grp = grdb_new_entry(grdb, rg->rg_name);
		if (!rg->rg_grp) { return -1; }
	}

	if (!rg->rg_sg) {
		rg->rg_sg = sgdb_new_entry(sgdb, rg->rg_name);
		if (!rg->rg_sg) { return -1; }
	}

	if (res_group_enforced(rg, PASSWD)) {
		rg->rg_grp->gr_passwd = strdup("x");
		rg->rg_sg->sg_passwd = strdup(rg->rg_passwd);
	}

	if (res_group_enforced(rg, GID)) {
		rg->rg_grp->gr_gid = rg->rg_gid;
	}

	if (res_group_enforced(rg, MEMBERS) && res_group_different(rg, MEMBERS)) {
		/* replace gr_mem and sg_mem with the rg_mem stringlist */
		xarrfree(rg->rg_grp->gr_mem);
		rg->rg_grp->gr_mem = xarrdup(rg->rg_mem->strings);

		xarrfree(rg->rg_sg->sg_mem);
		rg->rg_sg->sg_mem = xarrdup(rg->rg_mem->strings);
	}

	if (res_group_enforced(rg, ADMINS) && res_group_different(rg, ADMINS)) {
		/* replace sg_adm with the rg_adm stringlist */
		xarrfree(rg->rg_sg->sg_adm);
		rg->rg_sg->sg_adm = xarrdup(rg->rg_adm->strings);
	}

	return 0;
}
