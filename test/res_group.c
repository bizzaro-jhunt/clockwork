#include <stdlib.h>
#include <string.h>

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
	struct res_group *rg;
	rg = res_group_new();

	test("RES_GROUP: Default Enforcements");
	assert_true("NAME not enforced", !res_group_enforced(rg, NAME));
	assert_true("PASSWD not enforced", !res_group_enforced(rg, PASSWD));
	assert_true("GID not enforced", !res_group_enforced(rg, GID));

	test("RES_GROUP: NAME enforcement");
	ASSERT_ENFORCEMENT(rg,name,NAME,str,"name1","name2");

	test("RES_GROUP: PASSWD enforcement");
	ASSERT_ENFORCEMENT(rg,passwd,PASSWD,str,"password1","password2");

	test("RES_GROUP: GID enforcement");
	ASSERT_ENFORCEMENT(rg,gid,GID,int,15,16);

	test("RES_GROUP: Membership enforcement");
	res_group_enforce_members(rg, 1);
	assert_true("membership enforced", res_group_enforced(rg, MEMBERS));
	res_group_enforce_members(rg, 0);
	assert_true("membership no longer enforced", !res_group_enforced(rg, MEMBERS));

	test("RES_GROUP: Administrator enforcement");
	res_group_enforce_admins(rg, 1);
	assert_true("admin list enforced", res_group_enforced(rg, ADMINS));
	res_group_enforce_admins(rg, 0);
	assert_true("admin list no longer enforced", !res_group_enforced(rg, ADMINS));

	res_group_free(rg);
}

void test_res_group_merge()
{
	struct res_group *rg1, *rg2;

	rg1 = res_group_new();
	rg1->rg_prio = 10;

	rg2 = res_group_new();
	rg2->rg_prio = 20;

	res_group_set_gid(rg1, 909);
	res_group_set_name(rg1, "grp9");

	res_group_set_name(rg2, "fake-grp9");
	res_group_set_passwd(rg2, "grp9password");

	res_group_enforce_members(rg1, 1);
	res_group_add_member(rg1, "rg1-add1");
	res_group_add_member(rg1, "rg1-add2");

	res_group_enforce_members(rg2, 1);
	res_group_add_member(rg2, "rg2-add1");
	res_group_remove_member(rg2, "rg2-rm1");

	res_group_enforce_admins(rg2, 1);
	res_group_add_admin(rg2, "rg2-admin1");
	res_group_add_admin(rg2, "rg2-admin2");
	res_group_remove_admin(rg2, "former-admin");

	test("RES_GROUP: Merging group resources together");
	res_group_merge(rg1, rg2);
	assert_str_equals("NAME is set properly after merge", rg1->rg_name, "grp9");
	assert_str_equals("PASSWD is set properly after merge", rg1->rg_passwd, "grp9password");
	assert_int_equals("GID is set properly after merge", rg1->rg_gid, 909);

	assert_stringlist(rg1->rg_mem_add, "post-merge rg1->rg_mem_add", 3, "rg1-add1", "rg1-add2", "rg2-add1");
	assert_stringlist(rg1->rg_mem_rm,  "post-merge rg1->rg_mem_rm",  1, "rg2-rm1");
	assert_stringlist(rg1->rg_adm_add, "post-merge rg1->rg_adm_add", 2, "rg2-admin1", "rg2-admin2");
	assert_stringlist(rg1->rg_adm_rm,  "post-merge rg1->rg_adm_rm",  1, "former-admin");

	res_group_free(rg1);
	res_group_free(rg2);
}

void test_res_group_diffstat_remediation()
{
	struct res_group *rg;
	struct grdb *grdb;
	struct sgdb *sgdb;
	stringlist *list; /* for gr_mem / sg_mem / sg_adm tests */

	rg = res_group_new();

	res_group_set_name(rg, "service");
	res_group_set_gid(rg, 6000);

	/* real membership: account1, account2 */
	res_group_enforce_members(rg, 1);
	res_group_remove_member(rg, "account1");
	res_group_add_member(rg, "account2");
	res_group_add_member(rg, "account3");

	/* real admins: admin2 */
	res_group_enforce_admins(rg, 1);
	res_group_add_admin(rg, "admin1");
	res_group_remove_admin(rg, "admin2");

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
	assert_int_equals("res_group_stat returns 0", res_group_stat(rg, grdb, sgdb), 0);

	assert_true("NAME is in compliance", !res_group_different(rg, NAME));
	assert_true("GID is not in compliance", res_group_different(rg, GID));
	assert_true("MEMBERS is not in compliance", res_group_different(rg, MEMBERS));
	assert_true("ADMINS is not in compliance", res_group_different(rg, ADMINS));

	test("RES_GROUP: Remediation (existing account)");
	assert_int_equals("res_group_remediate returns 0", res_group_remediate(rg, grdb, sgdb), 0);
	assert_str_equals("rg_name is still set properly", rg->rg_grp->gr_name, "service");

	list = stringlist_new(rg->rg_grp->gr_mem);
	assert_stringlist(list, "post-remediation gr_mem", 2, "account2", "account3");
	stringlist_free(list);

	assert_str_equals("sg_namp is still set properly", rg->rg_sg->sg_namp, "service");
	assert_int_equals("rg_gid is updated properly", rg->rg_grp->gr_gid, 6000);

	list = stringlist_new(rg->rg_sg->sg_mem);
	assert_stringlist(list, "post-remediation sg_mem", 2, "account2", "account3");
	stringlist_free(list);

	list = stringlist_new(rg->rg_sg->sg_adm);
	assert_stringlist(list, "post-remediation sg_adm", 1, "admin1");
	stringlist_free(list);

	res_group_free(rg);
	grdb_free(grdb);
	sgdb_free(sgdb);
}

void test_res_group_remediate_new()
{
	struct res_group *rg;
	struct grdb *grdb;
	struct sgdb *sgdb;

	rg = res_group_new();

	res_group_set_name(rg, "new_group");
	res_group_set_gid(rg, 6010);

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
	assert_int_equals("res_group_stat returns 0", res_group_stat(rg, grdb, sgdb), 0);
	assert_int_equals("res_group_remediate returns 0", res_group_remediate(rg, grdb, sgdb), 0);
	assert_str_equals("rg_name is still set properly", rg->rg_grp->gr_name, "new_group");

	assert_str_equals("sg_namp is still set properly", rg->rg_sg->sg_namp, "new_group");
	assert_int_equals("rg_gid is updated properly", rg->rg_grp->gr_gid, 6010);

	res_group_free(rg);
	grdb_free(grdb);
	sgdb_free(sgdb);
}

void test_res_group_pack_detection()
{
	test("RES_FILE: pack detection based on tag");
	assert_int_equals("With leading tag", 0, res_group_is_pack("res_group::"));
	assert_int_not_equal("Bad tag (space issue)", 0, res_group_is_pack("res_group ::"));
	assert_int_not_equal("Bad tag (res_file)", 0, res_group_is_pack("res_file::"));
	assert_int_not_equal("Bad tag (random string)", 0, res_group_is_pack("this is an ARBITRARY string"));
}

void test_res_group_pack()
{
	struct res_group *rg;
	char *packed;
	const char *expected;

	rg = res_group_new();

	res_group_set_name(rg, "staff");
	res_group_set_passwd(rg, "sesame");
	res_group_set_gid(rg, 1415);
	res_group_add_member(rg, "admin1");
	res_group_add_member(rg, "admin2");
	res_group_add_member(rg, "admin3");
	res_group_add_member(rg, "admin4");
	res_group_remove_member(rg, "user");
	res_group_add_admin(rg, "admin1");

	test("RES_GROUP: pack res_group");
	packed = res_group_pack(rg);
	expected = "res_group::\"staff\"\"sesame\"00000587\"admin1.admin2.admin3.admin4\"\"user\"\"admin1\"\"\"";
	assert_str_equals("packs properly (normal case)", expected, packed);

	res_group_free(rg);
	free(packed);
}

void test_res_group_unpack()
{
	struct res_group *rg;
	char *packed;

	packed = "res_group::\"staff\"\"sesame\"00000587\"admin1.admin2.admin3.admin4\"\"user\"\"admin1\"\"\"";

	test("RES_GROUP: Unserialization");
	rg = res_group_unpack(packed);
	assert_not_null("res_group_pack succeeds", rg);
	assert_str_equals("res_group->rg_name is \"staff\"", "staff", rg->rg_name);
	assert_str_equals("res_group->rg_passwd is \"sesame\"", "sesame", rg->rg_passwd);
	assert_int_equals("res_group->rg_gid is 1415", 1415, rg->rg_gid);
	assert_stringlist(rg->rg_mem_add, "res_group->rg_mem_add", 4, "admin1", "admin2", "admin3", "admin4");
	assert_stringlist(rg->rg_mem_rm,  "res_group->rg_mem_rm",  1, "user");
	assert_stringlist(rg->rg_adm_add, "res_group->rg_adm_add", 1, "admin1");
	assert_stringlist(rg->rg_adm_rm,  "res_group->rg_adm_rm",  0);

	res_group_free(rg);
}

void test_suite_res_group() {
	test_res_group_enforcement();
	test_res_group_merge();
	test_res_group_diffstat_remediation();
	test_res_group_remediate_new();

	test_res_group_pack_detection();
	test_res_group_pack();
	test_res_group_unpack();
}
