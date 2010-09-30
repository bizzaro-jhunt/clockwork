#include <assert.h>
#include <string.h>
#include <errno.h>

#include "res_group.h"
#include "mem.h"

static int _res_group_diff(struct res_group *rg);

/*****************************************************************/

static int _res_group_diff(struct res_group *rg)
{
	assert(rg);
	assert(rg->rg_grp);
	assert(rg->rg_sg);

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

	rg->rg_enf = RES_GROUP_NONE;
	rg->rg_diff = RES_GROUP_NONE;
}

void res_group_free(struct res_group *rg)
{
	xfree(rg->rg_name);
	xfree(rg->rg_passwd);
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

	return 0;
}
