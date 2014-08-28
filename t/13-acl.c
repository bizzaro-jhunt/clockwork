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
#include "../src/mesh.h"

TESTS {
	subtest {
		acl_t *l = acl_new();
		isnt_null(l, "acl_new() allocated a new ACL");
		acl_destroy(l);
	}

	subtest {
		acl_t *l;

		l = acl_parse("allow user \"a b c\"");
		isnt_null(l, "parsed acl: allow user \"a b c\"");
		is_int(l->disposition, ACL_ALLOW, "dispositioned as an ALLOW rule");
		is_null(l->target_group, "target_group is not set for a user match");
		is_string(l->target_user, "user", "target_user is set for user match");
		ok(!l->is_final, "ACL is not final");
		acl_destroy(l);

		l = acl_parse("deny user \"do *\"");
		isnt_null(l, "parsed acl: deny user \"do *\"");
		is_int(l->disposition, ACL_DENY, "dispositioned as a DENY rule");
		is_null(l->target_group, "target_group is not set for a user match");
		is_string(l->target_user, "user", "target_user is set for user match");
		ok(l->is_final, "deny ACLs are always final");
		acl_destroy(l);

		l = acl_parse("allow %group \"*\"");
		isnt_null(l, "parsed acl: allow %%group \"*\"");
		is_int(l->disposition, ACL_ALLOW, "dispositioned as an ALLOW rule");
		is_string(l->target_group, "group", "target_group is set for a group match");
		is_null(l->target_user, "target_user is not set for group match");
		ok(!l->is_final, "ACL is not final");
		acl_destroy(l);

		l = acl_parse("deny %group \"do *\"");
		isnt_null(l, "parsed acl: deny %%group \"do *\"");
		is_int(l->disposition, ACL_DENY, "dispositioned as a DENY rule");
		is_string(l->target_group, "group", "target_group is set for a group match");
		is_null(l->target_user, "target_user is not set for group match");
		ok(l->is_final, "deny ACLs are always final");
		acl_destroy(l);

		l = acl_parse("allow admin * final");
		isnt_null(l, "parsed acl: allow admin * final");
		is_int(l->disposition, ACL_ALLOW, "dispositioned as an ALLOW rule");
		is_null(l->target_group, "target_group is not set for a user match");
		is_string(l->target_user, "admin" ,"target_user is set for user match");
		ok(l->is_final, "ACL is final");
		acl_destroy(l);
	}

	subtest { /* match user + command */
		acl_t *l;
		cmd_t *c;

		l = acl_parse("allow admin \"show *\"");
		isnt_null(l, "parsed acl: allow admin \"show *\"");

		c = cmd_parse("show version", COMMAND_LITERAL);
		isnt_null(c, "parsed command: show version");

		ok(acl_match(l, "admin", c),
			"acl applies to admin user running \"show version\"");
		ok(acl_match(l, "admin:group1:g2:g3", c),
			"acl still applies to admin, even with groups");

		ok(!acl_match(l, "adminX", c),
			"acl does not apply to adminX user (not the same as admin)");

		cmd_destroy(c);
		acl_destroy(l);
	}

	subtest { /* match group + command */
		acl_t *l;
		cmd_t *c;

		l = acl_parse("allow %staff \"show version\"");
		isnt_null(l, "parsed acl: allow %%staff \"show version\"");

		c = cmd_parse("show version", COMMAND_LITERAL);
		isnt_null(c, "parsed command: show version");

		ok(!acl_match(l, "admin", c),
			"acl doesn't apply to admin user with no groups");
		ok(!acl_match(l, "admin:sys", c),
			"acl doesn't apply to admin user not in staff group");

		ok(acl_match(l, "admin:staff", c),
			"acl applies to admin who is only in staff group");
		ok(acl_match(l, "admin:staff:g2:g3", c),
			"acl applies to admin in staff group and two others");
		ok(acl_match(l, "admin:g1:g2:staff", c),
			"group can show up at the end of the group list");
		ok(acl_match(l, "admin:g1:staff:g2", c),
			"group can show up in the middle of the group list");
		ok(acl_match(l, ":staff", c),
			"acl applies to any user in staff group");

		ok(!acl_match(l, "admin:flagstaff:staffers", c),
			"group name must match completely and exactly");

		cmd_destroy(c);
		acl_destroy(l);
	}

	subtest { /* match user but not command */
		acl_t *l;
		cmd_t *c;

		l = acl_parse("allow juser \"ping\"");
		isnt_null(l, "parsed acl: allow juser \"ping\"");

		c = cmd_parse("show version", COMMAND_LITERAL);
		isnt_null(c, "parsed command: show version");

		ok(!acl_match(l, "juser", c),
			"acl doesn't apply when the command doesn't match");

		cmd_destroy(c);
		acl_destroy(l);
	}

	subtest { /* match group but not command */
		acl_t *l;
		cmd_t *c;

		l = acl_parse("allow %dev \"ping\"");
		isnt_null(l, "parsed acl: allow %%dev \"ping\"");

		c = cmd_parse("show version", COMMAND_LITERAL);
		isnt_null(c, "parsed command: show version");

		ok(!acl_match(l, "juser:dev", c),
			"acl doesn't apply when the command doesn't match");

		cmd_destroy(c);
		acl_destroy(l);
	}

	subtest { /* match ignore command */
		acl_t *l;

		l = acl_parse("allow user1 \"show version\"");
		isnt_null(l, "parsed acl: allow user1 \"show version\"");
		ok(acl_match(l, "user1", NULL),
			"acl doesn't check the command if a NULL cmd_t pointer is given");
		ok(acl_match(l, "user1:group:dev:stuff", NULL),
			"acl doesn't check the command if a NULL cmd_t pointer is given");
		acl_destroy(l);

		l = acl_parse("allow %group \"show version\"");
		isnt_null(l, "parsed acl: allow group \"show version\"");
		ok(acl_match(l, "user1:group", NULL),
			"acl doesn't check the command if a NULL cmd_t pointer is given");
		ok(acl_match(l, "user1:group:dev:stuff", NULL),
			"acl doesn't check the command if a NULL cmd_t pointer is given");
		acl_destroy(l);
	}

	subtest { /* acls : simple case */
		LIST(acl);
		acl_t *l1, *l2, *l3;
		cmd_t *c;

		l1 = acl_parse("allow %sys   \"*\"");            isnt_null(l1, "1st acl");
		l2 = acl_parse("deny  %dev   \"show *\"");       isnt_null(l2, "2nd acl");
		l3 = acl_parse("allow juser  \"show version\""); isnt_null(l3, "3rd acl");

		cw_list_push(&acl, &l1->l);
		cw_list_push(&acl, &l2->l);
		cw_list_push(&acl, &l3->l);

		c = cmd_parse("show version", COMMAND_PATTERN);
		isnt_null(c, "command ok");

		is_int(acl_check(&acl, "user:group", c), ACL_NEUTRAL,
			"acl has no opinion on user:group running \"show version\"");

		is_int(acl_check(&acl, "user:sys", c), ACL_ALLOW,
			"acl allows user:sys to run \"show version\"");

		is_int(acl_check(&acl, "user:sys:dev", c), ACL_DENY,
			"allowed by %%sys, but denied by %%dev (because its show version)");

		is_int(acl_check(&acl, "juser:sys", c), ACL_ALLOW,
			"double allow for %%sys and explicit user grant");
		is_int(acl_check(&acl, "juser:dev", c), ACL_DENY,
			"implicit final on deny rules takes precedence over allow for juser");

		cmd_destroy(c);
		acl_destroy(l1);
		acl_destroy(l2);
		acl_destroy(l3);
	}

	subtest { /* acls : explicit final allow */
		LIST(acl);
		acl_t *l1, *l2, *l3;
		cmd_t *c;

		l1 = acl_parse("allow %sys   \"*\" final");      isnt_null(l1, "1st acl");
		l2 = acl_parse("deny  %dev   \"show *\"");       isnt_null(l2, "2nd acl");
		l3 = acl_parse("allow juser  \"show version\""); isnt_null(l3, "3rd acl");

		cw_list_push(&acl, &l1->l);
		cw_list_push(&acl, &l2->l);
		cw_list_push(&acl, &l3->l);

		c = cmd_parse("show version", COMMAND_PATTERN);
		isnt_null(c, "command ok");

		is_int(acl_check(&acl, "user:sys:dev", c), ACL_ALLOW,
			"final keyword on %%sys allow short-circuits the rest of the ACLs");

		cmd_destroy(c);
		acl_destroy(l1);
		acl_destroy(l2);
		acl_destroy(l3);
	}

	subtest { /* stringification */
		acl_t *a;
		char *s;

		a = acl_parse("allow user \"some command *\"");
		isnt_null(a, "parsed acl: allow user \"some command *\"");
		is_string(s = acl_string(a), "allow user \"some command *\"", "acl_string");
		acl_destroy(a); free(s);

		a = acl_parse("allow %group \"some command *\"");
		isnt_null(a, "parsed acl: allow %%group \"some command *\"");
		is_string(s = acl_string(a), "allow %group \"some command *\"", "acl_string");
		acl_destroy(a); free(s);

		a = acl_parse("allow user \"some command *\" final");
		isnt_null(a, "parsed acl: allow user \"some command *\" final");
		is_string(s = acl_string(a), "allow user \"some command *\" final", "acl_string");
		acl_destroy(a); free(s);

		a = acl_parse("allow %group \"some command *\" final");
		isnt_null(a, "parsed acl: allow %%group \"some command *\" final");
		is_string(s = acl_string(a), "allow %group \"some command *\" final", "acl_string");
		acl_destroy(a); free(s);

		a = acl_parse("deny user \"some command *\"");
		isnt_null(a, "parsed acl: deny user \"some command *\"");
		is_string(s = acl_string(a), "deny user \"some command *\"", "acl_string");
		acl_destroy(a); free(s);

		a = acl_parse("deny %group \"some command *\"");
		isnt_null(a, "parsed acl: deny %%group \"some command *\"");
		is_string(s = acl_string(a), "deny %group \"some command *\"", "acl_string");
		acl_destroy(a); free(s);

		a = acl_parse("deny user \"some command *\" final");
		isnt_null(a, "parsed acl: deny user \"some command *\" final");
		is_string(s = acl_string(a), "deny user \"some command *\"", "acl_string");
		acl_destroy(a); free(s);

		a = acl_parse("deny %group \"some command *\" final");
		isnt_null(a, "parsed acl: deny %%group \"some command *\" final");
		is_string(s = acl_string(a), "deny %group \"some command *\"", "acl_string");
		acl_destroy(a); free(s);
	}

	subtest { /* read acl files */
		LIST(acl);
		acl_t *a, *tmp;

		isnt_int(acl_read(&acl, "/path/to/nowhere"), 0, "Fail to read ENOENT file");
		ok(cw_list_isempty(&acl), "acl list is still empty after failed read");

		is_int(acl_read(&acl, TEST_TMP "/data/acl/empty.acl"), 0, "read empty.acl");
		ok(cw_list_isempty(&acl), "acl list is still empty (no acls in file)");

		is_int(acl_read(&acl, TEST_TMP "/data/acl/simple.acl"), 0, "read simple.acl");
		is_int(cw_list_len(&acl), 3, "parsed 3 acls from simple.acl");

		for_each_object_safe(a, tmp, &acl, l)
			acl_destroy(a);
		ok(cw_list_isempty(&acl), "acl is empty after freeing memory via acl_destroy");

		is_int(acl_read(&acl, TEST_TMP "/data/acl/comments.acl"), 0, "read comments.acl");
		is_int(cw_list_len(&acl), 2, "parsed 2 acls from comments.acl");
		for_each_object_safe(a, tmp, &acl, l)
			acl_destroy(a);
	}

	subtest { /* write acl files */
		LIST(acl);
		acl_t *l1, *l2, *l3;

		l1 = acl_parse("allow %sys   \"*\" final");      isnt_null(l1, "1st acl");
		l2 = acl_parse("deny  %dev   \"show *\"");       isnt_null(l2, "2nd acl");
		l3 = acl_parse("allow juser  \"show version\""); isnt_null(l3, "3rd acl");

		cw_list_push(&acl, &l1->l);
		cw_list_push(&acl, &l2->l);
		cw_list_push(&acl, &l3->l);

		isnt_int(acl_write(&acl, "/path/to/nowhere"), 0, "Fail to write to nonexistent file");
		is_int(acl_write(&acl, TEST_TMP "/acl.file"), 0, "Wrote to acl file");

		LIST(readback);
		is_int(acl_read(&readback, TEST_TMP "/acl.file"), 0, "Read the file we wrote");
		is_int(cw_list_len(&readback), 3, "Read 3 acls that we wrote");

		FILE *io = fopen(TEST_TMP "/acl.file", "r");
		char buf[8192];
		isnt_null(fgets(buf, 8191, io), "Read line from acl.file");
		is_string(buf, "# clockwork acl\n", "1st line is the identifiying comment");

		isnt_null(fgets(buf, 8191, io), "Read line from acl.file");
		is_string(buf, "allow %sys \"*\" final\n", "2nd line acl decl");

		isnt_null(fgets(buf, 8191, io), "Read line from acl.file");
		is_string(buf, "deny %dev \"show *\"\n", "2nd line acl decl");

		isnt_null(fgets(buf, 8191, io), "Read line from acl.file");
		is_string(buf, "allow juser \"show version\"\n", "2nd line acl decl");

		is_null(fgets(buf, 8191, io), "No more lines in acl.file");

		acl_t *a, *tmp;
		for_each_object_safe(a, tmp, &acl, l)
			acl_destroy(a);
		for_each_object_safe(a, tmp, &readback, l)
			acl_destroy(a);
	}

	done_testing();
}
