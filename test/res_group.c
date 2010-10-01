#include "test.h"
#include "../res_group.h"

#define ASSERT_ENFORCEMENT(o,f,c,t,v1,v2) do {\
	res_group_set_ ## f (o,v1); \
	assert_true( #c " enforced", res_group_enforced(o,c)); \
	assert_ ## t ## _equals( #c " set properly", (o)->rg_ ## f, v1); \
	\
	res_group_unset_ ## f (o); \
	assert_true( #c " no longer enforced", !res_group_enforced(o,c)); \
	\
	res_group_set_ ## f (o,v2); \
	assert_true( #c " re-enforced", res_group_enforced(o,c)); \
	assert_ ## t ## _equals ( #c " re-set properly", (o)->rg_ ## f, v2); \
} while(0)

void test_res_group_enforcement()
{
	struct res_group rg;
	res_group_init(&rg);

	test("RES_GROUP: Default Enforcements");
	assert_true("NAME not enforced", !res_group_enforced(&rg, NAME));
	assert_true("PASSWD not enforced", !res_group_enforced(&rg, PASSWD));
	assert_true("GID not enforced", !res_group_enforced(&rg, GID));

	test("RES_GROUP: NAME enforcement");
	ASSERT_ENFORCEMENT(&rg,name,NAME,str,"name1","name2");

	test("RES_GROUP: PASSWD enforcement");
	ASSERT_ENFORCEMENT(&rg,passwd,PASSWD,str,"password1","password2");

	test("RES_GROUP: GID enforcement");
	ASSERT_ENFORCEMENT(&rg,gid,GID,int,15,16);

	res_group_free(&rg);
}

void test_res_group_merge()
{
	struct res_group rg1, rg2;

	res_group_init(&rg1);
	rg1.rg_prio = 10;

	res_group_init(&rg2);
	rg2.rg_prio = 20;

	res_group_set_gid(&rg1, 909);
	res_group_set_name(&rg1, "grp9");

	res_group_set_name(&rg2, "fake-grp9");
	res_group_set_passwd(&rg2, "grp9password");

	test("RES_GROUP: Merging group resources together");
	res_group_merge(&rg1, &rg2);
	assert_str_equals("NAME is set properly after merge", rg1.rg_name, "grp9");
	assert_str_equals("PASSWD is set properly after merge", rg1.rg_passwd, "grp9password");
	assert_int_equals("GID is set properly after merge", rg1.rg_gid, 909);

	res_group_free(&rg1);
	res_group_free(&rg2);
}

void test_res_group_diffstat_remediation()
{
	struct res_group rg;
	struct grdb *grdb;
	struct sgdb *sgdb;

	res_group_init(&rg);

	res_group_set_name(&rg, "service");
	res_group_set_gid(&rg, 6000);

	grdb = grdb_init("test/data/group");
	if (!grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	sgdb = sgdb_init("test/data/gshadow");
	if (!sgdb) {
		assert_fail("Unable to init gshadow");
		return;
	}

	test("RES_GROUP: Diffstat");
	assert_int_equals("res_group_stat returns 0", res_group_stat(&rg, grdb, sgdb), 0);
	assert_true("NAME is in compliance", !res_group_different(&rg, NAME));
	assert_true("GID is not in compliance", res_group_different(&rg, GID));

	test("RES_GROUP: Remediation (existing account)");
	assert_int_equals("res_group_remediate returns 0", res_group_remediate(&rg, grdb, sgdb), 0);
	assert_str_equals("rg_name is still set properly", rg.rg_grp->gr_name, "service");

	assert_str_equals("sg_namp is still set properly", rg.rg_sg->sg_namp, "service");
	assert_int_equals("rg_gid is updated properly", rg.rg_grp->gr_gid, 6000);

	res_group_free(&rg);
	grdb_free(grdb);
	sgdb_free(sgdb);
}

void test_res_group_remediate_new()
{
	struct res_group rg;
	struct grdb *grdb;
	struct sgdb *sgdb;

	res_group_init(&rg);

	res_group_set_name(&rg, "new_group");
	res_group_set_gid(&rg, 6010);

	grdb = grdb_init("test/data/group");
	if (!grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	sgdb = sgdb_init("test/data/gshadow");
	if (!sgdb) {
		assert_fail("Unable to init gshadow");
		grdb_free(grdb);
		return;
	}

	test("RES_GROUP: Remediation (new account)");
	assert_int_equals("res_group_stat returns 0", res_group_stat(&rg, grdb, sgdb), 0);
	assert_int_equals("res_group_remediate returns 0", res_group_remediate(&rg, grdb, sgdb), 0);
	assert_str_equals("rg_name is still set properly", rg.rg_grp->gr_name, "new_group");

	assert_str_equals("sg_namp is still set properly", rg.rg_sg->sg_namp, "new_group");
	assert_int_equals("rg_gid is updated properly", rg.rg_grp->gr_gid, 6010);

	res_group_free(&rg);
	grdb_free(grdb);
	sgdb_free(sgdb);
}

void test_suite_res_group() {
	test_res_group_enforcement();
	test_res_group_merge();
	test_res_group_diffstat_remediation();
	test_res_group_remediate_new();
}
