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

#include "test.h"
#include "../src/clockwork.h"
#include "../src/resources.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define GROUP_DB   TEST_DATA "/userdb/group"
#define GSHADOW_DB TEST_DATA "/userdb/gshadow"

#define NEW_GROUP_DB   TEST_TMP "/group"
#define NEW_GSHADOW_DB TEST_TMP "/gshadow"

TESTS {
	subtest {
		struct res_group *group;
		char *key;

		isnt_null(group = res_group_new("group-key"), "create res_group");
		isnt_null(key = res_group_key(group), "got group key");
		is_string(key, "group:group-key", "group key");
		free(key);
		res_group_free(group);
	}

	subtest {
		ok(res_group_notify(NULL, NULL) == 0, "res_group_notify is a NOOP");
		ok(res_group_norm(NULL, NULL, NULL) == 0, "res_group_norm is a NOOP");
	}

	subtest {
		res_group_free(NULL);
		pass("res_group_free(NULL) doesn't segfault");
	}

	subtest {
		struct res_group *rg;
		isnt_null(rg = res_group_new("group1"), "created res_group");

		ok( ENFORCED(rg, RES_GROUP_NAME),   "NAME is enforced");
		ok(!ENFORCED(rg, RES_GROUP_PASSWD), "PASSWD is not enforced");
		ok(!ENFORCED(rg, RES_GROUP_GID),    "GID is not enforced");

		res_group_set(rg, "name", "name1");
		ok(ENFORCED(rg, RES_GROUP_NAME), "NAME is enforced");
		is_string(rg->rg_name, "name1", "name to enforce");

		res_group_set(rg, "password", "#$hash!@#$");
		res_group_set(rg, "changepw", "yes");
		ok(ENFORCED(rg, RES_GROUP_PASSWD), "PASSWD is enforced");
		is_string(rg->rg_passwd, "#$hash!@#$", "password to enforce");

		res_group_set(rg, "gid", "15");
		ok(ENFORCED(rg, RES_GROUP_GID), "GID is enforced");
		is_int(rg->rg_gid, 15, "GID to enforce");

		res_group_enforce_members(rg, 1);
		ok(ENFORCED(rg, RES_GROUP_MEMBERS), "MEMBERS is enforced");
		res_group_enforce_members(rg, 0);
		ok(!ENFORCED(rg, RES_GROUP_MEMBERS), "MEMBERS is not enforced");

		res_group_enforce_admins(rg, 1);
		ok(ENFORCED(rg, RES_GROUP_ADMINS), "ADMINS is enforced");
		res_group_enforce_admins(rg, 0);
		ok(!ENFORCED(rg, RES_GROUP_ADMINS), "ADMINS is not enforced");

		is_int(res_group_set(rg, "what-does-the-fox-say", "ring-ding-ring-ding"),
			-1, "res_group_set doesn't like nonsensical attributes");

		res_group_free(rg);
	}

	subtest {
		struct res_group *rg;

		rg = res_group_new("key");
		res_group_set(rg, "name", "group2");
		res_group_set(rg, "gid",  "1337");
		res_group_set(rg, "password", "sesame");
		res_group_set(rg, "changepw", "yes");

		ok(res_group_match(rg, "name", "group2")     == 0, "match name=group2");
		ok(res_group_match(rg, "gid",  "1337")       == 0, "match gid=1337");
		ok(res_group_match(rg, "name", "wheel")      != 0, "!match name=wheel");
		ok(res_group_match(rg, "gid",  "6")          != 0, "!match gid=6");
		ok(res_group_match(rg, "password", "sesame") != 0, "password is not a matchable attr");

		res_group_free(rg);
	}

	subtest {
		struct res_group *rg;

		rg = res_group_new("g1");

		res_group_set(rg, "member", "user1");
		res_group_set(rg, "member", "!user2");
		res_group_set(rg, "member", "!user3");
		res_group_set(rg, "member", "user3"); /* should move from _rm to _add */
		res_group_set(rg, "member", "user4");
		res_group_set(rg, "member", "!user4"); /* should move from _add to _rm */

		res_group_set(rg, "admin", "!admin1");
		res_group_set(rg, "admin", "admin2");
		res_group_set(rg, "admin", "!admin2"); /* should move from _add to rm */
		res_group_set(rg, "admin", "admin3");
		res_group_set(rg, "admin", "!admin4");
		res_group_set(rg, "admin", "admin4"); /* should move from _rm to _add */

		is_unsigned(rg->rg_mem_add->num, 2, "2 group members to add");
		is_string(rg->rg_mem_add->strings[0], "user1", "rg_mem_add[0]");
		is_string(rg->rg_mem_add->strings[1], "user3", "rg_mem_add[1]");
		is_null(rg->rg_mem_add->strings[2], "rg_mem_add[2] is NULL");

		is_unsigned(rg->rg_mem_rm->num, 2, "2 group members to remove");
		is_string(rg->rg_mem_rm->strings[0], "user2", "rg_mem_rm[0]");
		is_string(rg->rg_mem_rm->strings[1], "user4", "rg_mem_rm[1]");
		is_null(rg->rg_mem_rm->strings[2], "rg_mem_rm[2] is NULL");

		is_unsigned(rg->rg_adm_add->num, 2, "2 group admins to add");
		is_string(rg->rg_adm_add->strings[0], "admin3", "rg_adm_add[0]");
		is_string(rg->rg_adm_add->strings[1], "admin4", "rg_adm_add[1]");
		is_null(rg->rg_adm_add->strings[2], "rg_adm_add[2] is NULL");

		is_unsigned(rg->rg_adm_rm->num, 2, "2 group admins to remove");
		is_string(rg->rg_adm_rm->strings[0], "admin1", "rg_adm_rm[0]");
		is_string(rg->rg_adm_rm->strings[1], "admin2", "rg_adm_rm[1]");
		is_null(rg->rg_adm_rm->strings[2], "rg_adm_rm[2] is NULL");

		res_group_free(rg);
	}

	subtest {
		struct res_group *rg;
		struct hash *h;

		isnt_null(h = hash_new(), "created hash");
		isnt_null(rg = res_group_new("gr1"), "created group");

		ok(res_group_attrs(rg, h) == 0, "retrieved group attrs");
		is_string(hash_get(h, "name"),    "gr1", "h.name");
		is_string(hash_get(h, "present"), "yes", "h.present");
		is_null(hash_get(h, "gid"),      "h.gid is unset");
		is_null(hash_get(h, "password"), "h.password is unset");
		is_null(hash_get(h, "members"),  "h.members is unset");
		is_null(hash_get(h, "admins"),   "h.admins is unset");

		res_group_set(rg, "gid",      "707");
		res_group_set(rg, "password", "secret");
		res_group_set(rg, "changepw", "yes");

		ok(res_group_attrs(rg, h) == 0, "retrieved group attrs");
		is_string(hash_get(h, "name"),     "gr1",    "h.name");
		is_string(hash_get(h, "gid"),      "707",    "h.gid");
		is_string(hash_get(h, "password"), "secret", "h.password");

		res_group_set(rg, "member",  "u1");
		res_group_set(rg, "member",  "u2");
		res_group_set(rg, "member",  "!bad");
		res_group_set(rg, "member",  "!ex");
		res_group_set(rg, "admin",   "a1");
		res_group_set(rg, "admin",   "other");
		res_group_set(rg, "admin",   "!bad");
		res_group_set(rg, "admin",   "!them");
		res_group_set(rg, "present", "no");

		ok(res_group_attrs(rg, h) == 0, "retrieved group attrs");
		is_string(hash_get(h, "members"), "u1 u2 !bad !ex",      "h.members");
		is_string(hash_get(h, "admins"),  "a1 other !bad !them", "h.admins");
		is_string(hash_get(h, "present"), "no",                  "h.present");

		ok(res_group_set(rg, "xyzzy", "BAD") != 0, "xyzzy is not a valid attribute");
		ok(res_group_attrs(rg, h) == 0, "retrieved group attrs");
		is_null(hash_get(h, "xyzzy"), "h.xyzzy is unset (bad attr)");

		res_group_free(rg);
		hash_free_all(h);
	}

	done_testing();
}
