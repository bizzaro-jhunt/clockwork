/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/time.h>

#include "test.h"
#include "../../policy.h"
#include "../../resources.h"
#include "../../mem.h"
#include "../../spec/parser.h"

NEW_TEST(policy_creation)
{
	struct policy *pol;

	test("policy: Initialization of policy structure");
	pol = policy_new("policy name");
	assert_str_eq("Policy name is set correctly", pol->name, "policy name");
	assert_true("resources is an empty list head", list_empty(&pol->resources));

	policy_free(pol);
}

NEW_TEST(policy_resource_addition)
{
	struct policy *pol;
	struct resource *res1, *res2;

	test("policy: addition of resources to policy");
	pol = policy_new("test policy");
	assert_policy_has_users( "Policy (initially) has no users",  pol, 0);
	assert_policy_has_groups("Policy (initially) has no groups", pol, 0);
	assert_policy_has_files( "Policy (initially) has no files",  pol, 0);

	res1 = resource_new("user", "user1");
	assert_str_eq("User details are correct (before addition)", "user1",
		((struct res_user*)(res1->resource))->ru_name);
	policy_add_resource(pol, res1);

	res2 = resource_new("group", "group1");
	assert_str_eq("Group details are correct (before addition)", "group1",
		((struct res_group*)(res2->resource))->rg_name);
	policy_add_resource(pol, res2);

	assert_policy_has_files("Policy has no files", pol, 0);
	assert_policy_has_users("Policy has 1 user",   pol, 1);
	assert_policy_has_groups("Policy has 1 group", pol, 1);

	resource_free(res1);
	resource_free(res2);
	policy_free(pol);
}

NEW_TEST(policy_pack)
{
	struct policy *pol;
	struct resource *r;
	char *packed;

	test("policy: pack empty policy");
	pol = policy_new("empty");
	packed = policy_pack(pol);
	assert_not_null("policy_pack succeeds", packed);
	assert_str_eq("packed empty policy should be empty string",
		"policy::\"empty\"", packed);
	policy_free(pol);
	xfree(packed);

	test("policy: pack policy with one user, one group and one file");
	pol = policy_new("1 user, 1 group, and 1 file");
	/* The George Thoroughgood test */
	r = resource_new("user", "bourbon"); /* ru_enf == 0000 0001 */
	resource_set(r, "uid", "101");   /* ru_enf == 0000 0101 */
	resource_set(r, "gid", "2000");  /* ru_enf == 0000 1101 */
	policy_add_resource(pol, r);

	r = resource_new("group", "scotch"); /* rg_enf == 0000 0001 */
	resource_set(r, "gid", "2000");  /* rg_enf == 0000 0101 */
	policy_add_resource(pol, r);

	r = resource_new("file", "beer");              /* rf_enf == 0000 0000 */
	resource_set(r, "source", "/etc/issue");
	resource_set(r, "owner",  "george");       /* rf_enf == 0000 1001 */
	resource_set(r, "group",  "thoroughgood"); /* rf_enf == 0000 1011 */
	resource_set(r, "mode",   "0600");         /* rf_enf == 0000 1111 */
	policy_add_resource(pol, r);
	/* sneakily override the checksum */
	sha1_init(&((struct res_file*)(r->resource))->rf_rsha1,
	          "0123456789abcdef0123456789abcdef01234567");

	packed = policy_pack(pol);
	assert_not_null("policy_pack succeeds", packed);
	assert_str_eq("packed policy with 1 user, 1 group, and 1 file",
		"policy::\"1 user, 1 group, and 1 file\"\n"
		"res_user::\"bourbon\"0000000d\"bourbon\"\"\"" "00000065" "000007d0" "\"\"\"\"\"\"" "00" "\"\"" "01" "00000000" "00000000" "00000000" "00000000" "ffffffff\n"
		"res_group::\"scotch\"00000005\"scotch\"\"\"000007d0\"\"\"\"\"\"\"\"\n"
		"res_file::\"beer\"0000000f\"beer\"\"0123456789abcdef0123456789abcdef01234567\"" "\"george\"" "\"thoroughgood\"" "00000180",
		packed);

	free(packed);
	policy_free_all(pol);
}

NEW_TEST(policy_unpack)
{
	struct policy *pol;
	char *packed = \
		"policy::\"1 user, 1 group, and 1 file\"\n"
		"res_user::\"k1\"00003fff\"user1\"\"\"" "00000065" "000007d0" "\"\"\"\"\"\"" "00" "\"\"" "01" "00000000" "00000000" "00000000" "00000000" "ffffffff\n"
		"res_group::\"k2\"0000000f\"staff\"\"\"000007d0\"\"\"\"\"\"\"\"\n"
		"res_file::\"k3\"00000007\"\"\"cfm://etc/sudoers\"" "00000065" "000007d0" "00000180";

	test("policy: unpack empty policy");
	pol = policy_unpack("policy::\"empty\"00000309");
	assert_not_null("policy_unpack succeeds", pol);
	assert_true("resources is an empty list head", list_empty(&pol->resources));
	policy_free_all(pol);

	test("policy: unpack policy with one user, one group and one file");
	/* The George Thoroughgood test */
	pol = policy_unpack(packed);
	assert_not_null("policy_unpack succeeds", pol);
	assert_str_eq("policy name unpacked", "1 user, 1 group, and 1 file", pol->name);
	assert_true("resources is NOT an empty list head", !list_empty(&pol->resources));
	assert_policy_has_files("Policy has 1 file",   pol, 1);
	assert_policy_has_groups("Policy has 1 group", pol, 1);
	assert_policy_has_users("Policy has 1 user",   pol, 1);

	policy_free_all(pol);
}

NEW_TEST(policy_deps)
{
	struct manifest *m;
	struct policy *pol;
	struct hash *facts;

	test("policy: dependencies");
	facts = hash_new();
	m = parse_file(TEST_UNIT_DATA "/policy/norm/policy.pol");

	assert_not_null("Manifest parsed properly", m);
	pol = policy_generate(hash_get(m->policies, "base"), facts);
	assert_not_null("Policy 'base' found", pol);

	test("policy: file deps");
	assert_dep(pol, "file:test-file", "user:james");
	assert_dep(pol, "file:test-file", "group:staff");
	assert_dep(pol, "file:test-file", "dir:parent");

	test("policy: dir deps");
	assert_dep(pol, "dir:test-dir", "user:james");
	assert_dep(pol, "dir:test-dir", "group:staff");
	assert_dep(pol, "dir:test-dir", "dir:parent");

	assert_dep(pol, "dir:parent", "user:james");
	assert_dep(pol, "dir:parent", "group:staff");

	policy_free_all(pol);
	manifest_free(m);
}

NEW_SUITE(policy)
{
	RUN_TEST(policy_creation);

	RUN_TEST(policy_resource_addition);

	RUN_TEST(policy_pack);
	RUN_TEST(policy_unpack);

	RUN_TEST(policy_deps);
}
