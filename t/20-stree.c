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
#include "../src/policy.h"

static struct manifest *MANIFEST;

#define NODE(op,d1,d2) manifest_new_stree(MANIFEST, (op), cw_strdup(d1), cw_strdup(d2))

static struct stree* child_of(struct stree *parent, struct stree *new)
{
	if (!new) {
		BAIL_OUT("couldn't alloc a new stree node");
	}

	if (stree_add(parent, new) != 0) {
		BAIL_OUT("couldn't add stree to parent");
	}

	return new;
}

static cw_hash_t* facts_for_prog1(void)
{
	cw_hash_t *h = cw_alloc(sizeof(cw_hash_t));
	cw_hash_set(h, "test.users", "1");
	return h;
}

static cw_hash_t* facts_for_prog2(void)
{
	cw_hash_t *h = cw_alloc(sizeof(cw_hash_t));
	cw_hash_set(h, "test.users", "2");
	return h;
}

static struct stree* static_policy()
{
	struct stree *root, *node, *pol;

	root = NODE(PROG, NULL, NULL);
	if (!root) {
		return NULL;
	}

	pol = child_of(root, NODE(POLICY, "testing", NULL));

	node = child_of(pol, NODE(RESOURCE, "file", "/etc/sudoers"));
	child_of(node, NODE(ATTR, "owner", "root"));
	child_of(node, NODE(ATTR, "group", "root"));
	child_of(node, NODE(ATTR, "mode", "0600"));
	child_of(node, NODE(ATTR, "source", "std/etc-sudoers"));

	node = child_of(pol, NODE(RESOURCE, "user", "user1"));
	child_of(node, NODE(ATTR, "uid", "411"));
	child_of(node, NODE(ATTR, "gid", "1089"));
	child_of(node, NODE(ATTR, "home", "/srv/oper/info"));

	node = child_of(pol, NODE(RESOURCE, "group", "group54"));
	child_of(node, NODE(ATTR, "gid", "5454"));

	return root;
}

static struct stree* conditional_policy()
{
	struct stree *root, *node, *pol, *tmp;

	root = NODE(PROG, NULL, NULL);
	if (!root) {
		return NULL;
	}

	pol = child_of(root, NODE(POLICY, "testing", NULL));

	node = child_of(pol, NODE(RESOURCE, "file", "snmpd.conf"));
	child_of(node, NODE(ATTR, "owner", "root"));
	child_of(node, NODE(ATTR, "group", "root"));
	child_of(node, NODE(ATTR, "mode",  "0644"));

	tmp = child_of(node, NODE(IF, "sys.kernel.major", "2.6"));
	child_of(tmp, NODE(ATTR, "source", "std/2.6.conf"));
	child_of(tmp, NODE(ATTR, "source", "std/2.4.conf"));

	node = child_of(pol, NODE(IF, "lsb.distro.codename", "lucid"));
	/* if lucid... */
	tmp = child_of(node, NODE(RESOURCE, "user", "ubuntu"));
	child_of(tmp, NODE(ATTR, "uid", "20050"));
	child_of(tmp, NODE(ATTR, "gid", "20051"));
	child_of(tmp, NODE(ATTR, "home", "/srv/oper/ubuntu"));
	/* else */
	/* if karmic... */
	node = child_of(node, NODE(IF, "lsb.distro.codename", "karmic"));
	child_of(node, tmp);
	/* else... */
	child_of(node, NODE(NOOP, NULL, NULL));

	return root;
}

static struct stree* prog_policy()
{
	struct stree *root, *node, *pol, *res, *prog;

	root = NODE(PROG, NULL, NULL);
	if (!root) {
		return NULL;
	}

	pol = child_of(root, NODE(POLICY, "testing", NULL));

	/* if test.users == "2" */
	node = child_of(pol, NODE(IF, "test.users", "2"));
	prog = child_of(node, NODE(PROG, NULL, NULL));

		/* define group103 */
	res = child_of(prog, NODE(RESOURCE, "group", "group103"));
	child_of(res, NODE(ATTR, "gid", "103"));

		/* define group104 */
	res = child_of(prog, NODE(RESOURCE, "group", "group104"));
	child_of(res, NODE(ATTR, "gid", "104"));

	/* else */
	prog = child_of(node, NODE(PROG, NULL, NULL));

		/* define group101 */
	res = child_of(prog, NODE(RESOURCE, "group", "group101"));
	child_of(res, NODE(ATTR, "gid", "101"));

	return root;
}

static cw_hash_t* facts_for_lucid26(void)
{
	cw_hash_t *h = cw_alloc(sizeof(cw_hash_t));
	cw_hash_set(h, "lsb.distro.codename", "lucid");
	cw_hash_set(h, "sys.kernel.major",    "2.6");
	return h;
}

static cw_hash_t* facts_for_tikanga24(void)
{
	cw_hash_t *h = cw_alloc(sizeof(cw_hash_t));
	cw_hash_set(h, "sys.kernel.major",    "2.4");
	cw_hash_set(h, "lsb.distro.codename", "tikanga");
	return h;
}

TESTS {
	MANIFEST = manifest_new();

	subtest {
		struct stree *root, *child1, *child2;

		isnt_null(root = NODE(PROG, "progression", NULL), "root != NULL");
		is_int(root->size, 0, "root has no children");
		is_null(root->nodes, "root has no children");

		isnt_null(child1 = NODE(NOOP, "first",  "node"), "child1 != NULL");
		isnt_null(child2 = NODE(NOOP, "second", "node"), "child2 != NULL");

		ok(stree_add(root, child1) == 0, "added child1 to root");
		is_int(root->size, 1, "root now has 1 child node");
		ok(root->nodes[0] == child1, "child1 is the first child of root");

		ok(stree_add(root, child2) == 0, "added child2 to root");
		is_int(root->size, 2, "root now has 2 child nodes");
		ok(root->nodes[0] == child1, "child1 is the first child of root");
		ok(root->nodes[1] == child2, "child2 is the second child of root");
	}

	subtest {
		struct policy *pol;
		struct stree *root;
		unsigned int user_count, file_count, group_count;

		struct res_user *user;
		struct res_file *file;
		struct res_group *group;
		struct resource *r;

		isnt_null(root = static_policy(), "got a static policy");
		if (!root) break;

		isnt_null(pol = policy_generate(root, NULL), "generated policy");
		if (!pol) break;

		ok(!cw_list_isempty(&pol->resources), "policy defined has resources");

		user_count = group_count = file_count = 0;
		for_each_resource(r, pol) {
			switch (r->type) {
			case RES_USER:
				user_count++;
				user = (struct res_user*)(r->resource);
				isnt_null(user, "got the first res_user defined");
				if (user) {
					is_int(user->uid, 411, "user UID");
					is_int(user->gid, 1089, "user GID");
					is_string(user->dir, "/srv/oper/info", "user home");
				}
				break;

			case RES_FILE:
				file_count++;
				file = (struct res_file*)(r->resource);
				isnt_null(file, "got the first res_file defined");
				if (file) {
					is_int(file->mode, 0600, "file mode");
					is_string(file->rpath, "std/etc-sudoers", "file remote path");
					is_string(file->lpath, "/etc/sudoers", "file local path");
				}
				break;

			case RES_GROUP:
				group_count++;
				group = (struct res_group*)(r->resource);
				isnt_null(group, "got the first res_group defined");
				if (group) {
					is_int(group->gid, 5454, "group GID");
					is_string(group->name, "group54", "group name");
				}
				break;

			default:
				fail("Unknown resource type");
				break;

			}
		}
		ok(user_count  == 1, "only found 1 user");
		ok(file_count  == 1, "only found 1 file");
		ok(group_count == 1, "only found 1 group");

		policy_free_all(pol);
	}

	subtest {
		struct policy *pol;
		struct stree *root;
		cw_hash_t *facts;

		struct resource *r, *res;
		struct res_user *user;
		struct res_file *file;

		isnt_null(root = conditional_policy(), "got a conditional policy");
		if (!root) break;

		isnt_null(facts = facts_for_lucid26(), "generated Lucid 2.6 facts");
		isnt_null(pol = policy_generate(root, facts), "generated policy");
		if (!pol) break;

		is_int(num_res(pol, RES_USER),  1, "users defined");
		is_int(num_res(pol, RES_GROUP), 0, "groups defined");
		is_int(num_res(pol, RES_FILE),  1, "files defined");

		res = NULL;
		for_each_resource(r, pol) { if (r->type == RES_FILE) { res = r; break; } }
		isnt_null(res, "found a file resource");
		if (!res) break;
		isnt_null(file = (struct res_file*)(res->resource), "found first res_file");
		if (!file) break;
		is_int(file->mode, 0644, "file mode");
		is_string(file->rpath, "std/2.6.conf", "file remote path");
		is_string(file->lpath, "snmpd.conf", "file local path");

		res = NULL;
		for_each_resource(r, pol) { if (r->type == RES_USER) { res = r; break; } }
		isnt_null(res, "found a file resource");
		if (!res) break;
		isnt_null(user = (struct res_user*)(res->resource), "found first res_user");
		if (!user) break;
		is_int(user->uid, 20050, "user UID");
		is_int(user->gid, 20051, "user GID");
		is_string(user->dir, "/srv/oper/ubuntu", "user home");

		cw_hash_done(facts, 0);
		policy_free_all(pol);


		isnt_null(facts = facts_for_tikanga24(), "generated Tikanga 2.6 facts");
		isnt_null(pol = policy_generate(root, facts), "generated policy");
		if (!pol) break;

		is_int(num_res(pol, RES_USER),  0, "users defined");
		is_int(num_res(pol, RES_GROUP), 0, "groups defined");
		is_int(num_res(pol, RES_FILE),  1, "files defined");

		res = NULL;
		for_each_resource(r, pol) { if (r->type == RES_FILE) { res = r; break; } }
		isnt_null(res, "found a file resource");
		if (!res) break;
		isnt_null(file = (struct res_file*)(res->resource), "found first res_file");
		file = (struct res_file*)(res->resource);
		if (!file) break;
		is_int(file->mode, 0644, "file mode");
		is_string(file->rpath, "std/2.4.conf", "file remote path");
		is_string(file->lpath, "snmpd.conf", "file local path");

		cw_hash_done(facts, 0);
		policy_free_all(pol);
	}

	subtest {
		struct policy *pol;
		struct stree *root;
		cw_hash_t *facts;
		unsigned int i;

		struct resource *res, *r;
		struct res_group *group;

		isnt_null(root = prog_policy(), "got prog_policy definition");
		if (!root) break;

		isnt_null(facts = facts_for_prog1(), "got facts");
		isnt_null(pol = policy_generate(root, facts), "generated policy");
		if (!pol) break;
		is_int(num_res(pol, RES_GROUP), 1, "groups defined");

		res = NULL;
		for_each_resource(r, pol) { if (r->type == RES_GROUP) { res = r; break; } }
		isnt_null(res, "found group resources");
		if (!res) break;
		isnt_null(group = (struct res_group*)(res->resource), "found first res_group");
		if (!group) break;
		is_int(group->gid, 101, "group GID");

		cw_hash_done(facts, 0);
		policy_free_all(pol);

		isnt_null(facts = facts_for_prog2(), "got facts");
		isnt_null(pol = policy_generate(root, facts), "generated policy");
		if (!pol) break;
		is_int(num_res(pol, RES_GROUP), 2, "groups defined");

		i = 0;
		for_each_resource(r, pol) {
			if (r->type != RES_GROUP) { continue; }
			group = (struct res_group*)(r->resource);

			switch (i++) {
			case 0:
				isnt_null(group, "found first group");
				if (group) {
					is_int(group->gid, 103, "first group GID");
				}
				break;
			case 1:
				isnt_null(group, "found second group");
				if (group) {
					is_int(group->gid, 104, "second group GID");
				}
				break;
			}
		}
		is_int(i, 2, "tested both groups defined");

		cw_hash_done(facts, 0);
		policy_free_all(pol);
	}

	subtest {
		struct stree *a, *b, *c;

		a = static_policy();
		b = static_policy();
		c = conditional_policy();

		isnt_null(a, "stree a");
		isnt_null(b, "stree b");
		isnt_null(c, "stree c");
		if (!a | !b || !c) break;

		ok(stree_compare(a,b) == 0, "stree(a) == stree(b)");
		ok(stree_compare(b,c) != 0, "stree(b) != stree(c)");
		ok(stree_compare(a,c) != 0, "stree(a) != stree(c)");

		ok(stree_compare(a, NULL) != 0, "stree(a) != NULL");
		ok(stree_compare(NULL, b) != 0, "stree(b) != NULL");
		ok(stree_compare(NULL, NULL) != 0, "stree(NULL) != stree(NULL)");
	}

	manifest_free(MANIFEST);

	done_testing();
}
