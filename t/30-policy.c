/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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
#include "../src/spec/parser.h"

TESTS {
	subtest {
		struct policy *pol;

		isnt_null(pol = policy_new("policy name"), "initialized policy");
		is_string(pol->name, "policy name", "Policy name initialized");
		ok(list_isempty(&pol->resources), "new policy has no resources");

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
			((struct res_user*)(res1->resource))->name,
			"user1", 
			"username for new resource");
		policy_add_resource(pol, res1);

		isnt_null(res2 = resource_new("group", "group1"), "created group resource");
		is_string(
			((struct res_group*)(res2->resource))->name,
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
		struct manifest *m;
		struct policy *pol;
		hash_t *facts;

		facts = vmalloc(sizeof(hash_t));
		isnt_null(m = parse_file(TEST_DATA "/policy/parse.pol"),
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
		free(facts);
		manifest_free(m);
	}

	subtest {
		struct manifest *m;
		struct policy *pol;
		hash_t *facts;

		facts = vmalloc(sizeof(hash_t));
		isnt_null(m = parse_file(TEST_DATA "/policy/fail-unknown-attr.pol"),
				"manifest parsed");
		isnt_null(pol = policy_generate(hash_get(m->policies, "base"), facts),
				"policy 'base' found");

		policy_free_all(pol);
		free(facts);
		manifest_free(m);
	}

	done_testing();
}
