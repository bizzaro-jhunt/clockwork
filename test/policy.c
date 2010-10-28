#ifndef _TEST_POLICY_H
#define _TEST_POLICY_H

#include "test.h"
#include "../policy.h"

void test_policy_init()
{
	struct policy pol;

	test("POLICY: Initialization of policy structure");
	policy_init(&pol, "policy name", 12345);
	assert_str_equals("Policy name is set correctly", pol.name, "policy name");
	assert_int_equals("Policy version is set correctly", pol.version, 12345);
	assert_true("res_files is an empty list head", list_empty(&pol.res_files));
	assert_true("res_groups is an empty list head", list_empty(&pol.res_groups));
	assert_true("res_users is an empty list head", list_empty(&pol.res_users));

	policy_free(&pol);
}

void test_suite_policy()
{
	test_policy_init();
}

#endif
