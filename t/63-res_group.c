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

#include "test.h"
#include "../src/clockwork.h"
#include "../src/resources.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define GROUP_DB   "t/data/userdb/group"
#define GSHADOW_DB "t/data/userdb/gshadow"

#define NEW_GROUP_DB   "t/tmp/group"
#define NEW_GSHADOW_DB "t/tmp/gshadow"

int main(void) {
	test();

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
		struct report *report;
		struct resource_env env;

		rg = res_group_new("service");
		res_group_set(rg, "gid", "6000");

		/* real membership: account1, account2 */
		res_group_enforce_members(rg, 1);
		res_group_remove_member(rg, "account1");
		res_group_add_member(rg, "account2");
		res_group_add_member(rg, "account3");

		/* real admins: admin2 */
		res_group_enforce_admins(rg, 1);
		res_group_add_admin(rg, "admin1");
		res_group_remove_admin(rg, "admin2");

		env.group_grdb = grdb_init(GROUP_DB);
		if (!env.group_grdb) bail("failed to read group db");

		env.group_sgdb = sgdb_init(GSHADOW_DB);
		if (!env.group_sgdb) bail("failed to read gshadow db");

		ok(res_group_stat(rg, &env) == 0, "res_group_stat succeeds");

		ok(!DIFFERENT(rg, RES_GROUP_NAME),    "NAME is in compliance");
		ok( DIFFERENT(rg, RES_GROUP_GID),     "GID is out of compliance");
		ok( DIFFERENT(rg, RES_GROUP_MEMBERS), "MEMBERS is out of compliance");
		ok( DIFFERENT(rg, RES_GROUP_ADMINS),  "ADMINS is out of compliance");

		isnt_null(report = res_group_fixup(rg, 0, &env), "group fixup");
		is_int(report->fixed,     1, "group is fixed");
		is_int(report->compliant, 1, "group is compliant");

		is_string(rg->rg_grp->gr_name, "service", "group name still set");
		is_string(rg->rg_sg->sg_namp, "service", "gshadow entry name");
		is_int(rg->rg_grp->gr_gid, 6000, "group entry GID");

		is_string(rg->rg_grp->gr_mem[0], "account2", "gr_mem[0] == account2");
		is_string(rg->rg_grp->gr_mem[1], "account3", "gr_mem[1] == account3");
		is_null(rg->rg_grp->gr_mem[2], "gr_mem[2] is NULL");

		is_string(rg->rg_sg->sg_mem[0], "account2", "sg_mem[0] == account2");
		is_string(rg->rg_sg->sg_mem[1], "account3", "sg_mem[1] == account3");
		is_null(rg->rg_sg->sg_mem[2], "sg_mem[2] is NULL");

		is_string(rg->rg_sg->sg_adm[0], "admin1", "sg_adm[0] == admin1");
		is_null(rg->rg_sg->sg_adm[1], "sg_adm[1] is NULL");

		res_group_free(rg);
		grdb_free(env.group_grdb);
		sgdb_free(env.group_sgdb);
		report_free(report);
	}

	subtest {
		struct res_group *rg;
		struct report *report;
		struct resource_env env;

		rg = res_group_new("new_group");
		res_group_set(rg, "gid", "6010");

		env.group_grdb = grdb_init(GROUP_DB);
		if (!env.group_grdb) bail("failed to read group db");

		env.group_sgdb = sgdb_init(GSHADOW_DB);
		if (!env.group_sgdb) bail("failed to read gshadow db");

		ok(res_group_stat(rg, &env) == 0, "res_group_stat succeeds");
		isnt_null(report = res_group_fixup(rg, 0, &env), "group fixup");
		is_int(report->fixed,     1, "group is fixed");
		is_int(report->compliant, 1, "group is compliant");

		is_string(rg->rg_grp->gr_name, "new_group", "group entry name");
		is_string(rg->rg_sg->sg_namp,  "new_group", "gshadow entry name");
		is_int(rg->rg_grp->gr_gid,     6010,        "group entry GID");

		res_group_free(rg);
		grdb_free(env.group_grdb);
		sgdb_free(env.group_sgdb);
		report_free(report);
	}

	subtest {
		struct res_group *rg;
		struct report *report;
		struct resource_env env;
		struct resource_env env_after;

		rg = res_group_new("daemon");
		res_group_set(rg, "present", "no"); /* Remove the group */

		env.group_grdb = grdb_init(GROUP_DB);
		if (!env.group_grdb) bail("failed to read group db");

		env.group_sgdb = sgdb_init(GSHADOW_DB);
		if (!env.group_sgdb) bail("failed to read gshadow db");

		ok(res_group_stat(rg, &env) == 0, "res_group_stat succeeds");
		isnt_null(rg->rg_grp, "daemon group exists in group db");
		isnt_null(rg->rg_sg,  "daemon group exists in gshadow db");

		isnt_null(report = res_group_fixup(rg, 0, &env), "group fixup");
		is_int(report->fixed,     1, "group is fixed");
		is_int(report->compliant, 1, "group is compliant");

		ok(grdb_write(env.group_grdb, NEW_GROUP_DB)   == 0, "saved group db");
		ok(sgdb_write(env.group_sgdb, NEW_GSHADOW_DB) == 0, "saved gshadow db");

		env_after.group_grdb = grdb_init(NEW_GROUP_DB);
		if (!env_after.group_grdb) bail("failed to re-read group db");

		env_after.group_sgdb = sgdb_init(NEW_GSHADOW_DB);
		if (!env_after.group_sgdb) bail("failed to re-read gshadow db");

		res_group_free(rg);
		rg = res_group_new("daemon");

		ok(res_group_stat(rg, &env_after) == 0, "res_group_stat post-fixup");
		is_null(rg->rg_grp, "group entry doesnt exist post-fixup");
		is_null(rg->rg_sg,  "gshadow entry doesnt exist post-fixup");

		res_group_free(rg);

		grdb_free(env.group_grdb);
		sgdb_free(env.group_sgdb);
		grdb_free(env_after.group_grdb);
		sgdb_free(env_after.group_sgdb);
		report_free(report);
	}

	subtest {
		struct res_group *rg;
		struct report *report;
		struct resource_env env, env_after;

		rg = res_group_new("non_existent_group");
		res_group_set(rg, "present", "no"); /* Remove the group */

		env.group_grdb = grdb_init(GROUP_DB);
		if (!env.group_grdb) bail("failed to read group db");

		env.group_sgdb = sgdb_init(GSHADOW_DB);
		if (!env.group_sgdb) bail("failed to read gshadow db");

		ok(res_group_stat(rg, &env) == 0, "res_group_stat succeeds");
		is_null(rg->rg_grp, "non-existent group does not exist in group db");
		is_null(rg->rg_sg,  "non-existent group does not exist in gshadow db");

		isnt_null(report = res_group_fixup(rg, 0, &env), "res_group_fixup succeeds");
		is_int(report->fixed,     0, "group was already compliant");
		is_int(report->compliant, 1, "group is compliant");

		ok(grdb_write(env.group_grdb, NEW_GROUP_DB)   == 0, "saved group db");
		ok(sgdb_write(env.group_sgdb, NEW_GSHADOW_DB) == 0, "saved gshadow db");

		env_after.group_grdb = grdb_init(NEW_GROUP_DB);
		if (!env_after.group_grdb) bail("failed to re-read group db");

		env_after.group_sgdb = sgdb_init(NEW_GSHADOW_DB);
		if (!env_after.group_sgdb) bail("failed to re-read gshadow db");

		res_group_free(rg);
		rg = res_group_new("non_existent_group");

		ok(res_group_stat(rg, &env_after) == 0, "res_group_stat succeeds post-fixup");
		is_null(rg->rg_grp, "non-existent group does not exist in group db");
		is_null(rg->rg_sg,  "non-existent group does not exist in gshadow db");

		res_group_free(rg);

		grdb_free(env.group_grdb);
		sgdb_free(env.group_sgdb);
		grdb_free(env_after.group_grdb);
		sgdb_free(env_after.group_sgdb);
		report_free(report);
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
		char *packed;
		const char *expected;

		rg = res_group_new("staff");         /* rg_enf == 0000 0001 */
		res_group_set(rg, "password", "sesame"); /* rg_enf == UNCHANGED */
		res_group_set(rg, "changepw", "yes");    /* rg_enf == 0000 0011 */
		res_group_set(rg, "gid",      "1415");   /* rg_enf == 0000 0111 */
		res_group_add_member(rg, "admin1");  /* rg_enf == 0000 1111 */
		res_group_add_member(rg, "admin2");  /* ... */
		res_group_add_member(rg, "admin3");  /* ... */
		res_group_add_member(rg, "admin4");  /* ... */
		res_group_remove_member(rg, "user"); /* ... */
		res_group_add_admin(rg, "admin1");   /* rg_enf == 0001 1111 */

		packed = res_group_pack(rg);
		expected = "res_group::\"staff\"0000001f\"staff\"\"sesame\"00000587\"admin1.admin2.admin3.admin4\"\"user\"\"admin1\"\"\"";
		is_string(packed, expected, "group packs normally");

		res_group_free(rg);
		free(packed);
	}

	subtest {
		struct res_group *rg;
		char *packed;

		packed = "res_group::\"groupkey\"0000001d\"staff\"\"sesame\"00000587\"admin1.admin2.admin3.admin4\"\"user\"\"admin1\"\"\"";

		is_null(res_group_unpack("<invalid pack data>"), "res_group_unpack handles bad data");
		isnt_null(rg = res_group_unpack(packed), "res_group_unpack succeeds");

		is_string(rg->key,       "groupkey", "unpacked group key");
		is_string(rg->rg_name,   "staff",    "unpacked group name");
		is_string(rg->rg_passwd, "sesame",   "unpacked group password");
		is_int(   rg->rg_gid,    1415,       "unpacked group GID"); // 1415(10) == 0587(16)

		ok( ENFORCED(rg, RES_GROUP_NAME),    "NAME is enforced");
		ok( ENFORCED(rg, RES_GROUP_GID),     "GID is enforced");
		ok(!ENFORCED(rg, RES_GROUP_PASSWD),  "PASSWD is not enforced");
		ok( ENFORCED(rg, RES_GROUP_MEMBERS), "MEMBERS is enforced");
		ok( ENFORCED(rg, RES_GROUP_ADMINS),  "ADMINS is enforced");

		is_unsigned(rg->rg_mem_add->num, 4, "unpacked 4 group members to add");
		is_string(rg->rg_mem_add->strings[0], "admin1", "rg_mem_add[0]");
		is_string(rg->rg_mem_add->strings[1], "admin2", "rg_mem_add[1]");
		is_string(rg->rg_mem_add->strings[2], "admin3", "rg_mem_add[2]");
		is_string(rg->rg_mem_add->strings[3], "admin4", "rg_mem_add[3]");
		is_null(rg->rg_mem_add->strings[4], "rg_mem_add[4] is NULL");

		is_unsigned(rg->rg_mem_rm->num, 1, "unpacked 1 group member to remove");
		is_string(rg->rg_mem_rm->strings[0], "user", "rg_mem_rm[0]");
		is_null(rg->rg_mem_rm->strings[1], "rg_mem_rm[1] is NULL");

		is_unsigned(rg->rg_adm_add->num, 1, "unpacked 1 group admin to add");
		is_string(rg->rg_adm_add->strings[0], "admin1", "rg_mem_add[0]");
		is_null(rg->rg_adm_add->strings[1], "rg_adm_add[1] is NULL");

		is_unsigned(rg->rg_adm_rm->num, 0, "unpacked 0 group admins to remove");
		is_null(rg->rg_adm_rm->strings[0], "rg_adm_rm[0] is NULL");

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
		hash_free(h);
	}

	subtest { /* cfm-20 - create a group with a conflicting GID */
		struct res_group *rg;
		struct report *report;
		struct resource_env env;
		struct resource_env env_after;

		rg = res_group_new("conflictd");
		res_group_set(rg, "present", "yes");
		res_group_set(rg, "gid", "3"); /* conflict with sys group */

		env.group_grdb = grdb_init(GROUP_DB);
		if (!env.group_grdb) bail("failed to read group db");

		env.group_sgdb = sgdb_init(GSHADOW_DB);
		if (!env.group_sgdb) bail("failed to read gshadow db");

		ok(res_group_stat(rg, &env) == 0, "res_group_stat succeeds");

		isnt_null(report = res_group_fixup(rg, 0, &env), "group fixup");

		is_int(report->fixed,     0, "group is not fixed");
		is_int(report->compliant, 0, "group is non-compliant");

		ok(grdb_write(env.group_grdb, NEW_GROUP_DB)   == 0, "saved group db");
		ok(sgdb_write(env.group_sgdb, NEW_GSHADOW_DB) == 0, "saved gshadow db");

		env_after.group_grdb = grdb_init(NEW_GROUP_DB);
		if (!env_after.group_grdb) bail("failed to re-read group db");

		env_after.group_sgdb = sgdb_init(NEW_GSHADOW_DB);
		if (!env_after.group_sgdb) bail("failed to re-read gshadow db");

		is_null(grdb_get_by_name(env_after.group_grdb, "conflictd"),
			"group conflictd should not have been created in grdb");
		is_null(sgdb_get_by_name(env_after.group_sgdb, "conflictd"),
			"group conflictd should not have been created in sgdb");
	}

	subtest { /* cfm-20 - edit an existing group with a conflicting GID */
		struct res_group *rg;
		struct report *report;
		struct resource_env env;
		struct resource_env env_after;

		rg = res_group_new("daemon");
		res_group_set(rg, "present", "yes");
		res_group_set(rg, "gid", "3"); /* conflict with sys group */

		env.group_grdb = grdb_init(GROUP_DB);
		if (!env.group_grdb) bail("failed to read group db");

		env.group_sgdb = sgdb_init(GSHADOW_DB);
		if (!env.group_sgdb) bail("failed to read gshadow db");

		ok(res_group_stat(rg, &env) == 0, "res_group_stat succeeds");

		isnt_null(report = res_group_fixup(rg, 0, &env), "group fixup");

		is_int(report->fixed,     0, "group is not fixed");
		is_int(report->compliant, 0, "group is non-compliant");

		ok(grdb_write(env.group_grdb, NEW_GROUP_DB)   == 0, "saved group db");
		ok(sgdb_write(env.group_sgdb, NEW_GSHADOW_DB) == 0, "saved gshadow db");

		env_after.group_grdb = grdb_init(NEW_GROUP_DB);
		if (!env_after.group_grdb) bail("failed to re-read group db");

		env_after.group_sgdb = sgdb_init(NEW_GSHADOW_DB);
		if (!env_after.group_sgdb) bail("failed to re-read gshadow db");

		struct group *gr = grdb_get_by_name(env_after.group_grdb, "daemon");
		isnt_null(gr, "daemon group should still exist in group db");
		is_int(gr->gr_gid, 1, "daemon group GID unchanged");
	}

	done_testing();
}
