#include <sys/time.h>

#include "test.h"
#include "assertions.h"
#include "../policy.h"
#include "../mem.h"

void test_policy_creation()
{
	struct policy *pol;

	test("policy: Initialization of policy structure");
	pol = policy_new("policy name", 12345);
	assert_str_equals("Policy name is set correctly", pol->name, "policy name");
	assert_int_equals("Policy version is set correctly", pol->version, 12345);
	assert_true("res_files is an empty list head", list_empty(&pol->res_files));
	assert_true("res_groups is an empty list head", list_empty(&pol->res_groups));
	assert_true("res_users is an empty list head", list_empty(&pol->res_users));

	policy_free(pol);
}

void test_policy_latest_version()
{
	struct policy *pol;
	struct timeval tv;

	test("policy: generating latest version as a timestamp");

	pol = policy_new("current", policy_latest_version());
	gettimeofday(&tv, NULL);

	assert_unsigned_equals("Latest version is seconds since epoch (var)", tv.tv_sec, pol->version);

	policy_free(pol);

}

void test_policy_user_addition()
{
	struct policy *pol;
	struct res_user *ru;
	struct res_user *ptr;
	size_t n = 0;

	pol = policy_new("user policy", 1234);

	test("policy: addition of users to policy");
	ru = res_user_new();
	res_user_set_name(ru, "user1");
	assert_str_equals("User details are correct (before addition)", "user1", ru->ru_name);
	policy_add_user_resource(pol, ru);

	n = 0;
	for_each_node(ptr, &pol->res_users, res) {
		if (n++ == 0) {
			assert_str_equals("User details are correct", "user1", ptr->ru_name);
		}
	}
	assert_int_equals("Only one user in the list", 1, n);

	res_user_free(ru);
	policy_free(pol);
}

void test_policy_group_addition()
{
	struct policy *pol;
	struct res_group *rg;
	struct res_group *ptr;
	size_t n = 0;

	pol = policy_new("group policy", 1234);

	test("policy: addition of groups to policy");
	rg = res_group_new();
	res_group_set_name(rg, "group1");
	assert_str_equals("Group details are correct (before addition)", "group1", rg->rg_name);
	policy_add_group_resource(pol, rg);

	n = 0;
	for_each_node(ptr, &pol->res_groups, res) {
		if (n++ == 0) {
			assert_str_equals("Group details are correct", "group1", ptr->rg_name);
		}
	}
	assert_int_equals("Only one group in the list", 1, n);

	res_group_free(rg);
	policy_free(pol);
}

void test_policy_file_addition()
{
	struct policy *pol;
	struct res_file *rf;
	struct res_file *ptr;
	size_t n = 0;

	pol = policy_new("file policy", 1234);

	test("policy: addition of files to policy");
	rf = res_file_new();
	res_file_set_source(rf, "file1");
	assert_str_equals("File details are correct (before addition)", "file1", rf->rf_rpath);
	policy_add_file_resource(pol, rf);

	n = 0;
	for_each_node(ptr, &pol->res_files, res) {
		if (n++ == 0) {
			assert_str_equals("File details are correct", "file1", ptr->rf_rpath);
		}
	}
	assert_int_equals("Only one file in the list", 1, n);

	res_file_free(rf);
	policy_free(pol);
}

void test_policy_pack()
{
	struct policy *pol;
	char *packed;

	struct res_user  *ru1;
	struct res_group *rg1;
	struct res_file  *rf1;

	test("policy: pack empty policy");
	pol = policy_new("empty", 777); // 777 is 309 in hex
	packed = policy_pack(pol);
	assert_not_null("policy_pack succeeds", packed);
	assert_str_equals("packed empty policy should be empty string",
		"policy::\"empty\"00000309", packed);
	policy_free(pol);
	xfree(packed);

	test("policy: pack policy with one user, one group and one file");
	pol = policy_new("1 user, 1 group, and 1 file", 20101031);
	/* The George Thoroughgood test */
	ru1 = res_user_new();
	res_user_set_uid(ru1, 101);
	res_user_set_gid(ru1, 2000);
	res_user_set_name(ru1, "user1");
	policy_add_user_resource(pol, ru1);

	rg1 = res_group_new();
	res_group_set_gid(rg1, 2000);
	res_group_set_name(rg1, "staff");
	policy_add_group_resource(pol, rg1);

	rf1 = res_file_new();
	res_file_set_source(rf1, "cfm://etc/sudoers");
	res_file_set_uid(rf1, 101);
	res_file_set_gid(rf1, 2000);
	res_file_set_mode(rf1, 0600);
	policy_add_file_resource(pol, rf1);

	packed = policy_pack(pol);
	assert_not_null("policy_pack succeeds", packed);
	assert_str_equals("packed policy with 1 user, 1 group, and 1 file",
		"policy::\"1 user, 1 group, and 1 file\"0132b7a7\n" /* 0132b7z7 = 20101031 decimal */
		"res_user::\"user1\"\"\"" "00000065" "000007d0" "\"\"\"\"\"\"" "00" "\"\"" "01" "00000000" "00000000" "00000000" "00000000" "00000000\n"
		"res_group::\"staff\"\"\"000007d0\"\"\"\"\"\"\"\"\n"
		"res_file::\"\"\"cfm://etc/sudoers\"" "00000065" "000007d0" "00000180",
		packed);

	free(packed);

	res_user_free(ru1);
	res_group_free(rg1);
	res_file_free(rf1);
	policy_free(pol);
}

void test_policy_unpack()
{
	struct policy *pol;
	char *packed = \
		"policy::\"1 user, 1 group, and 1 file\"0132b7a7\n" /* 0132b7z7 = 20101031 decimal */
		"res_user::\"user1\"\"\"" "00000065" "000007d0" "\"\"\"\"\"\"" "00" "\"\"" "01" "00000000" "00000000" "00000000" "00000000" "00000000\n"
		"res_group::\"staff\"\"\"000007d0\"\"\"\"\"\"\"\"\n"
		"res_file::\"\"\"cfm://etc/sudoers\"" "00000065" "000007d0" "00000180";

	test("policy: unpack empty policy");
	pol = policy_unpack("policy::\"empty\"00000309");
	assert_not_null("policy_unpack succeeds", pol);
	assert_true("res_files is an empty list head", list_empty(&pol->res_files));
	assert_true("res_groups is an empty list head", list_empty(&pol->res_groups));
	assert_true("res_users is an empty list head", list_empty(&pol->res_users));
	policy_free(pol);

	test("policy: unpack policy with one user, one group and one file");
	/* The George Thoroughgood test */
	pol = policy_unpack(packed);
	assert_not_null("policy_unpack succeeds", pol);
	assert_str_equals("policy name unpacked", "1 user, 1 group, and 1 file", pol->name);
	assert_unsigned_equals("policy version unpacked", 20101031, pol->version);
	assert_true("res_files is NOT an empty list head", !list_empty(&pol->res_files));
	assert_true("res_groups is NOT an empty list head", !list_empty(&pol->res_groups));
	assert_true("res_users is NOT an empty list head", !list_empty(&pol->res_users));

	policy_free_all(pol);
}

void test_suite_policy()
{
	test_policy_creation();
	test_policy_latest_version();

	test_policy_user_addition();
	test_policy_group_addition();
	test_policy_file_addition();

	test_policy_pack();
	test_policy_unpack();
}
