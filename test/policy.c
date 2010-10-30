#ifndef _TEST_POLICY_H
#define _TEST_POLICY_H

#include <string.h>
#include <stdio.h>

#include "test.h"
#include "../policy.h"
#include "../mem.h"

void test_policy_creation()
{
	struct policy *pol;

	test("POLICY: Initialization of policy structure");
	pol = policy_new("policy name", 12345);
	assert_str_equals("Policy name is set correctly", pol->name, "policy name");
	assert_int_equals("Policy version is set correctly", pol->version, 12345);
	assert_true("res_files is an empty list head", list_empty(&pol->res_files));
	assert_true("res_groups is an empty list head", list_empty(&pol->res_groups));
	assert_true("res_users is an empty list head", list_empty(&pol->res_users));

	policy_free(pol);
}

void test_policy_user_addition()
{
	struct policy *pol;
	struct res_user *ru;
	struct res_user *ptr;
	size_t n = 0;

	pol = policy_new("user policy", 1234);

	test("POLICY: addition of users to policy");
	ru = res_user_new(ru);
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
	struct res_group rg;
	struct res_group *ptr;
	size_t n = 0;

	pol = policy_new("group policy", 1234);

	test("POLICY: addition of groups to policy");
	res_group_init(&rg);
	res_group_set_name(&rg, "group1");
	assert_str_equals("Group details are correct (before addition)", "group1", rg.rg_name);
	policy_add_group_resource(pol, &rg);

	n = 0;
	for_each_node(ptr, &pol->res_groups, res) {
		if (n++ == 0) {
			assert_str_equals("Group details are correct", "group1", ptr->rg_name);
		}
	}
	assert_int_equals("Only one group in the list", 1, n);

	res_group_free(&rg);
	policy_free(pol);
}

void test_policy_file_addition()
{
	struct policy *pol;
	struct res_file rf;
	struct res_file *ptr;
	size_t n = 0;

	pol = policy_new("file policy", 1234);

	test("POLICY: addition of files to policy");
	res_file_init(&rf);
	res_file_set_source(&rf, "file1");
	assert_str_equals("File details are correct (before addition)", "file1", rf.rf_rpath);
	policy_add_file_resource(pol, &rf);

	n = 0;
	for_each_node(ptr, &pol->res_files, res) {
		if (n++ == 0) {
			assert_str_equals("File details are correct", "file1", ptr->rf_rpath);
		}
	}
	assert_int_equals("Only one file in the list", 1, n);

	res_file_free(&rf);
	policy_free(pol);
}

void test_policy_serialization()
{
	struct policy *pol;
	char *policy_str;
	size_t len;

	struct res_user *ru1;
	struct res_group rg1;
	struct res_file rf1;

	test("POLICY: Serialization of empty policy");
	pol = policy_new("serialized policy", 12346);
	assert_int_equals("policy_serialize returns 0", 0, policy_serialize(pol, &policy_str, &len));
	assert_str_equals("Serialized empty policy should be empty string", "", policy_str);
	xfree(policy_str);

	test("POLICY: Serialization with one user, one group and one file");
	/* The George Thoroughgood test */
	ru1 = res_user_new();
	res_user_set_uid(ru1, 101);
	res_user_set_gid(ru1, 2000);
	res_user_set_name(ru1, "user1");
	policy_add_user_resource(pol, ru1);

	res_group_init(&rg1);
	res_group_set_gid(&rg1, 2000);
	res_group_set_name(&rg1, "staff");
	policy_add_group_resource(pol, &rg1);

	res_file_init(&rf1);
	res_file_set_source(&rf1, "cfm://etc/sudoers");
	res_file_set_uid(&rf1, 101);
	res_file_set_gid(&rf1, 2000);
	res_file_set_mode(&rf1, 0600);
	policy_add_file_resource(pol, &rf1);

	assert_int_equals("policy_serialize returns 0", 0, policy_serialize(pol, &policy_str, &len));
	assert_str_equals("Serialized policy with 1 user, 1 group, and 1 file",
		"res_user {\"user1\":\"\":\"101\":\"2000\":\"\":\"\":\"\":\"0\":\"\":\"1\":\"0\":\"0\":\"0\":\"0\":\"0\"}\n"
		"res_group {\"staff\":\"\":\"2000\":\"\":\"\":\"\":\"\"}\n"
		"res_file {\"\":\"cfm://etc/sudoers\":\"101\":\"2000\":\"384\"}",
		policy_str);

	policy_free(pol);
	res_user_free(ru1);
	res_group_free(&rg1);
	res_file_free(&rf1);
	xfree(policy_str);
}

void test_policy_unserialization()
{
	struct policy *pol;
	char *policy_str = \
		"res_user {\"user1\":\"\":\"101\":\"2000\":\"\":\"\":\"\":\"0\":\"\":\"1\":\"0\":\"0\":\"0\":\"0\":\"0\"}\n"
		"res_group {\"staff\":\"\":\"2000\":\"\":\"\":\"\":\"\"}\n"
		"res_file {\"\":\"cfm://etc/sudoers\":\"101\":\"2000\":\"384\"}";

	size_t len = strlen(policy_str);

	struct res_user  *ru, *ru_tmp;
	struct res_group *rg, *rg_tmp;
	struct res_file  *rf, *rf_tmp;

	test("POLICY: Unserialization of empty policy");
	pol = policy_new("empty policy", 12346);
	assert_int_equals("policy_unserialize returns 0", 0, policy_unserialize(pol, "", 0));
	assert_true("res_files is an empty list head", list_empty(&pol->res_files));
	assert_true("res_groups is an empty list head", list_empty(&pol->res_groups));
	assert_true("res_users is an empty list head", list_empty(&pol->res_users));
	policy_free(pol);

	test("POLICY: Unserialization with one user, one group and one file");
	/* The George Thoroughgood test */
	pol = policy_new("george thoroughgood policy", 12346);
	assert_int_equals("policy_unserialize returns 0", 0, policy_unserialize(pol, policy_str, len));
	assert_true("res_files is NOT an empty list head", !list_empty(&pol->res_files));
	assert_true("res_groups is NOT an empty list head", !list_empty(&pol->res_groups));
	assert_true("res_users is NOT an empty list head", !list_empty(&pol->res_users));

	for_each_node_safe(ru, ru_tmp, &pol->res_users,  res) { res_user_free(ru); }
	for_each_node_safe(rg, rg_tmp, &pol->res_groups, res) { res_group_free(rg); free(rg); }
	for_each_node_safe(rf, rf_tmp, &pol->res_files,  res) { res_file_free(rf);  free(rf); }
	policy_free(pol);
}

void test_suite_policy()
{
	test_policy_creation();

	test_policy_user_addition();
	test_policy_group_addition();
	test_policy_file_addition();

	test_policy_serialization();
	test_policy_unserialization();
}

#endif
