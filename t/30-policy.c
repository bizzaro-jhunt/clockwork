/*
  Copyright 2011-2014 James Hunt <james@niftylogic.com>

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
#include "../src/policy.h"
#include "../src/resources.h"
#include "../src/mem.h"
#include "../src/spec/parser.h"

TESTS {
	subtest {
		struct policy *pol;

		isnt_null(pol = policy_new("policy name"), "initialized policy");
		is_string(pol->name, "policy name", "Policy name initialized");
		ok(list_empty(&pol->resources), "new policy has no resources");

		policy_free(pol);
	}

	subtest {
		struct policy *pol;
		struct resource *res1, *res2;

		isnt_null(pol = policy_new("test policy"), "created test policy");
		is_int(num_res(pol, RES_USER),  0, "user resources");
		is_int(num_res(pol, RES_GROUP), 0, "group resources");
		is_int(num_res(pol, RES_FILE),  0, "file resources");

		isnt_null(res1 = resource_new("user", "user1"), "created use resource");
		is_string(
			((struct res_user*)(res1->resource))->ru_name,
			"user1", 
			"username for new resource");
		policy_add_resource(pol, res1);

		isnt_null(res2 = resource_new("group", "group1"), "created group resource");
		is_string(
			((struct res_group*)(res2->resource))->rg_name,
			"group1",
			"group name for new resource");
		policy_add_resource(pol, res2);

		is_int(num_res(pol, RES_USER),  1, "user resources");
		is_int(num_res(pol, RES_GROUP), 1, "group resources");
		is_int(num_res(pol, RES_FILE),  0, "file resources");

		resource_free(res1);
		resource_free(res2);
		policy_free(pol);
	}

	subtest {
		struct policy *pol;
		struct resource *r;
		char *packed;

		isnt_null(pol = policy_new("empty"), "generated empty policy");
		isnt_null(packed = policy_pack(pol), "policy_pack succeeds");
		is_string(packed, "policy::\"empty\"", "packed empty policy");
		policy_free(pol);
		xfree(packed);

		isnt_null(pol = policy_new("1 user, 1 group, and 1 file"), "generated policy");
		/* The George Thoroughgood test */
		r = resource_new("user", "bourbon"); /* ru_enf == 0000 0001 */
		resource_set(r, "uid", "101");   /* ru_enf == 0000 0101 */
		resource_set(r, "gid", "2000");  /* ru_enf == 0000 1101 */
		policy_add_resource(pol, r);

		r = resource_new("user", "whiskey"); /* ru_enf == 0000 0001 */
		resource_set(r, "password", "sour");
		resource_set(r, "changepw", "yes");
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

		isnt_null(packed = policy_pack(pol), "packed policy");
		is_string(packed,
			"policy::\"1 user, 1 group, and 1 file\"\n"
			"res_user::\"bourbon\"0000000d\"bourbon\"\"*\"" "00000065" "000007d0" "\"\"\"\"\"\"" "00" "\"\"" "01" "00000000" "00000000" "00000000" "00000000" "ffffffff\n"
			"res_user::\"whiskey\"0000000f\"whiskey\"\"sour\"" "00000065" "000007d0" "\"\"\"\"\"\"" "00" "\"\"" "01" "00000000" "00000000" "00000000" "00000000" "ffffffff\n"
			"res_group::\"scotch\"00000005\"scotch\"\"\"000007d0\"\"\"\"\"\"\"\"\n"
			"res_file::\"beer\"0000000f\"beer\"\"0123456789abcdef0123456789abcdef01234567\"" "\"george\"" "\"thoroughgood\"" "00000180",

			"packed policy successfully");

		free(packed);
		policy_free_all(pol);
	}

	subtest {
		struct policy *pol;
		char *packed = \
			"policy::\"1 user, 1 group, and 1 file\"\n"
			"res_user::\"k1\"00003fff\"user1\"\"\"" "00000065" "000007d0" "\"\"\"\"\"\"" "00" "\"\"" "01" "00000000" "00000000" "00000000" "00000000" "ffffffff\n"
			"res_group::\"k2\"0000000f\"staff\"\"\"000007d0\"\"\"\"\"\"\"\"\n"
			"res_file::\"k3\"00000007\"\"\"cfm://etc/sudoers\"" "00000065" "000007d0" "00000180";

		isnt_null(pol = policy_unpack("policy::\"empty\"00000309"), "unpacked empty policy");
		ok(list_empty(&pol->resources), "resources of empty policy is empty list");
		policy_free_all(pol);

		/* The George Thoroughgood test */
		isnt_null(pol = policy_unpack(packed), "unpacked policy");
		is_string(pol->name, "1 user, 1 group, and 1 file", "policy name unpacked");
		ok(!list_empty(&pol->resources), "unpacked policy has resources");
		is_int(num_res(pol, RES_USER),  1, "user resources");
		is_int(num_res(pol, RES_GROUP), 1, "group resources");
		is_int(num_res(pol, RES_FILE),  1, "fileresources");

		policy_free_all(pol);
	}

	subtest {
		struct manifest *m;
		struct policy *pol;
		struct hash *facts;

		facts = hash_new();
		isnt_null(m = parse_file(TEST_DATA "/policy/norm/policy.pol"),
				"manifest parsed");
		isnt_null(pol = policy_generate(hash_get(m->policies, "base"), facts),
				"policy 'base' found");

		ok(has_dep(pol, "file:test-file", "user:james"),
				"file:test-file -> user:james");
		ok(has_dep(pol, "file:test-file", "group:staff"),
				"file:test-file -> group:staff");
		ok(has_dep(pol, "file:test-file", "dir:parent"),
				"file:test-file -> dir:parent");

		ok(has_dep(pol, "dir:test-dir", "user:james"),
				"dir:test-dir -> user:james");
		ok(has_dep(pol, "dir:test-dir", "group:staff"),
				"dir:test-dir -> group:staff");
		ok(has_dep(pol, "dir:test-dir", "dir:parent"),
				"dir:test-dir -> dir:parent");

		ok(has_dep(pol, "dir:parent", "user:james"),
				"dir:parent -> user:james");
		ok(has_dep(pol, "dir:parent", "group:staff"),
				"dir:parent -> group:staff");

		policy_free_all(pol);
		manifest_free(m);
	}

	subtest {
		struct manifest *m;
		struct policy *pol;
		struct hash *facts;

		facts = hash_new();
		isnt_null(m = parse_file(TEST_DATA "/policy/fail/unknown-attr.pol"),
				"manifest parsed");
		isnt_null(pol = policy_generate(hash_get(m->policies, "base"), facts),
				"policy 'base' found");

		policy_free_all(pol);
		manifest_free(m);
	}

	done_testing();
}
