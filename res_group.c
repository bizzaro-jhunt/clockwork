#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "res_group.h"
#include "mem.h"

static int _res_group_diff(struct res_group *rg);

/*****************************************************************/

static int _res_group_diff(struct res_group *rg)
{
	assert(rg);

	rg->rg_diff = RES_GROUP_NONE;

	if (res_group_enforced(rg, NAME) && strcmp(rg->rg_name, rg->rg_grp.gr_name) != 0) {
		rg->rg_diff |= RES_GROUP_NAME;
	}

	if (res_group_enforced(rg, PASSWD) && strcmp(rg->rg_passwd, rg->rg_grp.gr_passwd) != 0) {
		rg->rg_diff |= RES_GROUP_PASSWD;
	}

	if (res_group_enforced(rg, GID) && rg->rg_gid != rg->rg_grp.gr_gid) {
		rg->rg_diff |= RES_GROUP_GID;
	}

	return 0;
}

/*****************************************************************/

void res_group_init(struct res_group *rg)
{
	rg->rg_enf = 0;
	rg->rg_prio = 0;

	rg->rg_name = NULL;
	rg->rg_passwd = NULL;
	rg->rg_gid = 0;
}

void res_group_free(struct res_group *rg)
{
	xfree(rg->rg_name);
	xfree(rg->rg_passwd);
}

int res_group_set_name(struct res_group *rg, const char *name)
{
	assert(rg);

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
		printf("Overriding NAME of rg1 with value from rg2\n");
		res_group_set_name(rg1, rg2->rg_name);
	}

	if ( res_group_enforced(rg2, PASSWD) &&
	    !res_group_enforced(rg1, PASSWD)) {
		printf("Overriding PASSWD of rg1 with value from rg2\n");
		res_group_set_passwd(rg1, rg2->rg_passwd);
	}

	if ( res_group_enforced(rg2, GID) &&
	    !res_group_enforced(rg1, GID)) {
		printf("Overriding GID of rg1 with value from rg2\n");
		res_group_set_gid(rg1, rg2->rg_gid);
	}
}

int res_group_stat(struct res_group *rg)
{
	assert(rg);

	struct group *entry = NULL;

	/* getgrgid and getgrnam return NULL on error OR no match.
	   clear errno manually to test for errors. */
	errno = 0;

	if (res_group_enforced(rg, GID)) {
		printf("Looking for group by GID (%u)\n", rg->rg_gid);
		entry = getgrgid(rg->rg_gid);
		if (!entry && errno) { return -1; }
	}

	if (!entry && res_group_enforced(rg, NAME)) {
		printf("Looking for group by name (%s)\n", rg->rg_name);
		entry = getgrnam(rg->rg_name);
		if (!entry && errno) { return -1; }
	}

	if (entry) {
		/* entry may point to static storage cf. getgrnam(3); */
		memcpy(&rg->rg_grp, entry, sizeof(struct group));
	}

	return _res_group_diff(rg);
}

void res_group_dump(struct res_group *rg)
{
	printf("\n\n");
	printf("struct res_group (0x%0x) {\n", (unsigned int)rg);
	printf("    rg_name: \"%s\"\n", rg->rg_name);
	printf("  rg_passwd: \"%s\"\n", rg->rg_passwd);
	printf("     rg_gid: %u\n", rg->rg_gid);
	printf("--- (rg_grp omitted) ---\n");

	printf("      rg_grp: struct passwd {\n");
	printf("                 gr_name: \"%s\"\n", rg->rg_grp.gr_name);
	printf("               gr_passwd: \"%s\"\n", rg->rg_grp.gr_passwd);
	printf("                  gr_gid: %u\n", rg->rg_grp.gr_gid);
	printf("             }\n");

	printf("     rg_enf: %o\n", rg->rg_enf);
	printf("    rg_diff: %o\n", rg->rg_diff);
	printf("}\n");
	printf("\n");

	printf("NAME:   %s (%02o & %02o == %02o)\n",
	       (res_group_enforced(rg, NAME) ? "enforced  " : "unenforced"),
	       rg->rg_enf, RES_GROUP_NAME, rg->rg_enf & RES_GROUP_NAME);
	printf("PASSWD: %s (%02o & %02o == %02o)\n",
	       (res_group_enforced(rg, PASSWD) ? "enforced  " : "unenforced"),
	       rg->rg_enf, RES_GROUP_PASSWD, rg->rg_enf & RES_GROUP_PASSWD);
	printf("GID:    %s (%02o & %02o == %02o)\n",
	       (res_group_enforced(rg, GID) ? "enforced  " : "unenforced"),
	       rg->rg_enf, RES_GROUP_GID, rg->rg_enf & RES_GROUP_GID);
}
