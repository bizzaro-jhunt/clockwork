#include "test.h"
#include "../res_group.h"

static void assert_rg_name_enforcement(struct res_group *rg)
{
	const char *name1 = "name1";
	const char *name2 = "name2";

	res_group_set_name(rg, name1);
	assert_true("NAME enforced", res_group_enforced(rg, NAME));
	assert_str_equals("NAME set properly", rg->rg_name, name1);

	res_group_unset_name(rg);
	assert_true("NAME no longer enforced", !res_group_enforced(rg, NAME));

	res_group_set_name(rg, name2);
	assert_str_equals("NAME re-set properly", rg->rg_name, name2);
}

static void assert_rg_passwd_enforcement(struct res_group *rg)
{
	const char *passwd1 = "password1";
	const char *passwd2 = "password2";

	res_group_set_passwd(rg, passwd1);
	assert_true("PASSWD enforced", res_group_enforced(rg, PASSWD));
	assert_str_equals("PASSWD set properly", rg->rg_passwd, passwd1);

	res_group_unset_passwd(rg);
	assert_true("PASSWD no longer enforced", !res_group_enforced(rg, PASSWD));

	res_group_set_passwd(rg, passwd2);
	assert_str_equals("PASSWD re-set properly", rg->rg_passwd, passwd2);
}


static void assert_rg_gid_enforcement(struct res_group *rg)
{
	res_group_set_gid(rg, 22);
	assert_true("GID enforced", res_group_enforced(rg, GID));
	assert_int_equals("GID set properly", rg->rg_gid, 22);

	res_group_unset_gid(rg);
	assert_true("GID no longer enforced", !res_group_enforced(rg, GID));

	res_group_set_gid(rg, 46);
	assert_int_equals("GID re-set roperly", rg->rg_gid, 46);
}

void test_res_group_enforcement()
{
	struct res_group rg;
	res_group_init(&rg);

	test("RES_GROUP: Default Enforcements");
	assert_true("NAME not enforced", !res_group_enforced(&rg, NAME));
	assert_true("GID not enforced", !res_group_enforced(&rg, GID));

	test("RES_GROUP: NAME enforcement");
	assert_rg_name_enforcement(&rg);

	test("RES_GROUP: PASSWD enforcement");
	assert_rg_passwd_enforcement(&rg);

	test("RES_GROUP: GID enforcement");
	assert_rg_gid_enforcement(&rg);

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

void test_suite_res_group() {
	test_res_group_enforcement();
	test_res_group_merge();
}
