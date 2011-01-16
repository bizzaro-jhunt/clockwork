#include "test.h"
#include "assertions.h"
#include "../res_group.h"

#define ASSERT_ENFORCEMENT(o,f,c,t,v1,v2) do {\
	res_group_set_ ## f (o,v1); \
	assert_true( #c " enforced", res_group_enforced(o,c)); \
	assert_ ## t ## _equals( #c " set properly", (o)->rg_ ## f, v1); \
} while(0)

void test_res_group_enforcement()
{
	struct res_group *rg;
	rg = res_group_new("group1");

	test("RES_GROUP: Default Enforcements");
	assert_true("NAME enforced",        res_group_enforced(rg, NAME));
	assert_true("PASSWD not enforced", !res_group_enforced(rg, PASSWD));
	assert_true("GID not enforced",    !res_group_enforced(rg, GID));

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

void test_res_group_diffstat_remediation()
{
	struct res_group *rg;
	struct grdb *grdb;
	struct sgdb *sgdb;
	stringlist *list; /* for gr_mem / sg_mem / sg_adm tests */
	struct report *report;

	rg = res_group_new("service");
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
	report = res_group_remediate(rg, 0, grdb, sgdb);
	assert_not_null("res_group_remediate returns a report", report);
	assert_int_equals("group is fixed", report->fixed, 1);
	assert_int_equals("group is now compliant", report->compliant, 1);

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
	report_free(report);
}

void test_res_group_remediate_new()
{
	struct res_group *rg;
	struct grdb *grdb;
	struct sgdb *sgdb;
	struct report *report;

	rg = res_group_new("new_group");
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
	report = res_group_remediate(rg, 0, grdb, sgdb);
	assert_not_null("res_group_remediate returns a report", report);
	assert_int_equals("group is fixed", report->fixed, 1);
	assert_int_equals("group is now compliant", report->compliant, 1);

	assert_str_equals("rg_name is still set properly", rg->rg_grp->gr_name, "new_group");

	assert_str_equals("sg_namp is still set properly", rg->rg_sg->sg_namp, "new_group");
	assert_int_equals("rg_gid is updated properly", rg->rg_grp->gr_gid, 6010);

	res_group_free(rg);
	grdb_free(grdb);
	sgdb_free(sgdb);
	report_free(report);
}

void test_res_group_remediate_remove_existing()
{
	struct res_group *rg;
	struct grdb *grdb, *grdb_after;
	struct sgdb *sgdb, *sgdb_after;
	struct report *report;

	rg = res_group_new("daemon");
	res_group_set_presence(rg, 0); /* Remove the group */

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

	test("RES_GROUP: Remediation (remove existing account)");
	assert_int_equals("res_group_stat returns 0", res_group_stat(rg, grdb, sgdb), 0);
	assert_not_null("(test sanity) group found in group file",   rg->rg_grp);
	assert_not_null("(test sanity) group found in gshadow file", rg->rg_sg);

	report = res_group_remediate(rg, 0, grdb, sgdb);
	assert_not_null("res_group_remediate returns a report", report);
	assert_int_equals("group is fixed", report->fixed, 1);
	assert_int_equals("group is now compliant", report->compliant, 1);

	assert_int_equals("grdb_write succeeds", 0, grdb_write(grdb, "test/tmp/group.new"));
	assert_int_equals("sgdb_write succeeds", 0, sgdb_write(sgdb, "test/tmp/gshadow.new"));

	grdb_after = grdb_init("test/tmp/group.new");
	if (!grdb_after) {
		assert_fail("Unable to init grdb_after");
		return;
	}

	sgdb_after = sgdb_init("test/tmp/gshadow.new");
	if (!sgdb_after) {
		assert_fail("Unable to init sgdb_after");
		grdb_free(grdb_after);
		return;
	}

	res_group_free(rg);
	rg = res_group_new("non_existent_group");

	assert_int_equals("res_group_stat returns zero (after)", res_group_stat(rg, grdb_after, sgdb_after), 0);
	assert_null("No group entry exists after remediation", rg->rg_grp);
	assert_null("No gshadow entry exists after remediation", rg->rg_sg);

	res_group_free(rg);

	grdb_free(grdb);
	sgdb_free(sgdb);
	grdb_free(grdb_after);
	sgdb_free(sgdb_after);
	report_free(report);
}

void test_res_group_remediate_remove_nonexistent()
{
	struct res_group *rg;
	struct grdb *grdb, *grdb_after;
	struct sgdb *sgdb, *sgdb_after;
	struct report *report;

	rg = res_group_new("non_existent_group");
	res_group_set_presence(rg, 0); /* Remove the group */

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

	test("RES_GROUP: Remediation (remove nonexistent account)");
	assert_int_equals("res_group_stat returns 0", res_group_stat(rg, grdb, sgdb), 0);
	assert_null("(test sanity) group not found in group file",   rg->rg_grp);
	assert_null("(test sanity) group not found in gshadow file", rg->rg_sg);

	report = res_group_remediate(rg, 0, grdb, sgdb);
	assert_not_null("res_group_remediate returns a report", report);
	assert_int_equals("group was already compliant", report->fixed, 0);
	assert_int_equals("group is now compliant", report->compliant, 1);

	assert_int_equals("grdb_write succeeds", 0, grdb_write(grdb, "test/tmp/group.new"));
	assert_int_equals("sgdb_write succeeds", 0, sgdb_write(sgdb, "test/tmp/gshadow.new"));

	grdb_after = grdb_init("test/tmp/group.new");
	if (!grdb_after) {
		assert_fail("Unable to init grdb_after");
		return;
	}

	sgdb_after = sgdb_init("test/tmp/gshadow.new");
	if (!sgdb_after) {
		assert_fail("Unable to init sgdb_after");
		grdb_free(grdb_after);
		return;
	}

	res_group_free(rg);
	rg = res_group_new("non_existent_group");

	assert_int_equals("res_group_stat returns zero (after)", res_group_stat(rg, grdb_after, sgdb_after), 0);
	assert_null("No group entry exists after remediation", rg->rg_grp);
	assert_null("No gshadow entry exists after remediation", rg->rg_sg);

	res_group_free(rg);

	grdb_free(grdb);
	sgdb_free(sgdb);
	grdb_free(grdb_after);
	sgdb_free(sgdb_after);
	report_free(report);
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

	rg = res_group_new("staff");         /* rg_enf == 0000 0001 */
	res_group_set_passwd(rg, "sesame");  /* rg_enf == 0000 0011 */
	res_group_set_gid(rg, 1415);         /* rg_enf == 0000 0111 */
	res_group_add_member(rg, "admin1");  /* rg_enf == 0000 1111 */
	res_group_add_member(rg, "admin2");  /* ... */
	res_group_add_member(rg, "admin3");  /* ... */
	res_group_add_member(rg, "admin4");  /* ... */
	res_group_remove_member(rg, "user"); /* ... */
	res_group_add_admin(rg, "admin1");   /* rg_enf == 0001 1111 */

	test("RES_GROUP: pack res_group");
	packed = res_group_pack(rg);
	expected = "res_group::0000001f\"staff\"\"sesame\"00000587\"admin1.admin2.admin3.admin4\"\"user\"\"admin1\"\"\"";
	assert_str_equals("packs properly (normal case)", expected, packed);

	res_group_free(rg);
	free(packed);
}

void test_res_group_unpack()
{
	struct res_group *rg;
	char *packed;

	packed = "res_group::0000001d\"staff\"\"sesame\"00000587\"admin1.admin2.admin3.admin4\"\"user\"\"admin1\"\"\"";

	test("RES_GROUP: Unserialization");
	rg = res_group_unpack(packed);
	assert_not_null("res_group_unpack succeeds", rg);
	assert_str_equals("res_group->rg_name is \"staff\"", "staff", rg->rg_name);
	assert_true("'name' is enforced", res_group_enforced(rg, NAME));

	assert_str_equals("res_group->rg_passwd is \"sesame\"", "sesame", rg->rg_passwd);
	assert_true("'password' is NOT enforced", !res_group_enforced(rg, PASSWD));

	assert_int_equals("res_group->rg_gid is 1415", 1415, rg->rg_gid); // 1415(10) == 0587(16)
	assert_true("'gid' is enforced", res_group_enforced(rg, GID));

	assert_stringlist(rg->rg_mem_add, "res_group->rg_mem_add", 4, "admin1", "admin2", "admin3", "admin4");
	assert_stringlist(rg->rg_mem_rm,  "res_group->rg_mem_rm",  1, "user");
	assert_true("'members' is enforced", res_group_enforced(rg, MEMBERS));

	assert_stringlist(rg->rg_adm_add, "res_group->rg_adm_add", 1, "admin1");
	assert_stringlist(rg->rg_adm_rm,  "res_group->rg_adm_rm",  0);
	assert_true("'admins' is enforced", res_group_enforced(rg, ADMINS));

	res_group_free(rg);
}

void test_suite_res_group() {
	test_res_group_enforcement();
	test_res_group_diffstat_remediation();
	test_res_group_remediate_new();
	test_res_group_remediate_remove_existing();
	test_res_group_remediate_remove_nonexistent();

	test_res_group_pack_detection();
	test_res_group_pack();
	test_res_group_unpack();
}
