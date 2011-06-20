#include "test.h"
#include "assertions.h"
#include "../policy.h"

static struct manifest *MANIFEST;

#define NODE(op,d1,d2) manifest_new_stree(MANIFEST, (op), (d1), (d2))

struct stree* child_of(struct stree *parent, struct stree *new)
{
	if (!new) {
		perror("couldn't alloc an stree node");
		exit(1);
	}

	if (stree_add(parent, new) != 0) {
		fprintf(stderr, "couldn't add stree to parent\n");
		exit(1);
	}

	return new;
}

void test_stree_child_nodes()
{
	struct stree *root, *child1, *child2;

	test("stree: child node management");
	root = NODE(PROG, "progression", NULL);
	assert_not_null("root is a valid stree pointer", root);
	assert_int_eq("initially, root has no children", 0, root->size);
	assert_null("initially, root has no children", root->nodes);

	child1 = NODE(NOOP, "first", "node");
	assert_not_null("child1 is a valid stree pointer", child1);

	child2 = NODE(NOOP, "second", "node");
	assert_not_null("child2 is a valid stree pointer", child2);

	assert_int_eq("Adding child1 to root works", 0, stree_add(root, child1));
	assert_int_eq("After adding child1, root has one node", 1, root->size);
	assert_true("After adding child1, root->nodes[0] is child1", root->nodes[0] == child1);

	assert_int_eq("Adding child2 to root works", 0, stree_add(root, child2));
	assert_int_eq("After adding child2, root has two nodes", 2, root->size);
	assert_true("After adding child2, root->nodes[1] is child2", root->nodes[1] == child2);

	/* memory deallocation handled by test suite cleanup code */
}

static struct stree* static_policy()
{
	struct stree *root, *node;

	root = NODE(PROG, NULL, NULL);
	if (!root) {
		return NULL;
	}

	node = child_of(root, NODE(RESOURCE, "file", "/etc/sudoers"));
	child_of(node, NODE(ATTR, "owner", "root"));
	child_of(node, NODE(ATTR, "group", "root"));
	child_of(node, NODE(ATTR, "mode", "0600"));
	child_of(node, NODE(ATTR, "source", "std/etc-sudoers"));

	node = child_of(root, NODE(RESOURCE, "user", "user1"));
	child_of(node, NODE(ATTR, "uid", "411"));
	child_of(node, NODE(ATTR, "gid", "1089"));
	child_of(node, NODE(ATTR, "home", "/srv/oper/info"));

	node = child_of(root, NODE(RESOURCE, "group", "group54"));
	child_of(node, NODE(ATTR, "gid", "5454"));

	return root;
}

static struct stree* conditional_policy()
{
	struct stree *root, *node, *tmp;

	root = NODE(PROG, NULL, NULL);
	if (!root) {
		return NULL;
	}

	node = child_of(root, NODE(RESOURCE, "file", "snmpd.conf"));
	child_of(node, NODE(ATTR, "owner", "root"));
	child_of(node, NODE(ATTR, "group", "root"));
	child_of(node, NODE(ATTR, "mode",  "0644"));

	tmp = child_of(node, NODE(IF, "sys.kernel.major", "2.6"));
	child_of(tmp, NODE(ATTR, "source", "std/2.6.conf"));
	child_of(tmp, NODE(ATTR, "source", "std/2.4.conf"));

	node = child_of(root, NODE(IF, "lsb.distro.codename", "lucid"));
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

static struct hash* facts_for_lucid26(void)
{
	struct hash *h = hash_new();
	hash_set(h, "lsb.distro.codename", "lucid");
	hash_set(h, "sys.kernel.major",    "2.6");
	return h;
}

static struct hash* facts_for_tikanga24(void)
{
	struct hash *h = hash_new();
	hash_set(h, "sys.kernel.major",    "2.4");
	hash_set(h, "lsb.distro.codename", "tikanga");
	return h;
}


void test_stree_static_policy_generation()
{
	struct policy *pol;
	struct stree *root;
	unsigned int user_count, file_count, group_count;

	struct res_user *user;
	struct res_file *file;
	struct res_group *group;
	struct resource *r;

	test("stree: static stree generates static policy with empty list");
	root = static_policy();
	assert_not_null("(test sanity) static_policy() should return an stree pointer", root);

	pol = policy_generate(root, NULL);
	assert_not_null("policy defined successfully from stree", pol);

	assert_true("policy defined has resources", !list_empty(&pol->resources));

	user_count = group_count = file_count = 0;
	for_each_resource(r, pol) {
		switch (r->type) {
		case RES_USER:
			user_count++;
			user = (struct res_user*)(r->resource);
			assert_not_null("got the first res_user defined", user);
			assert_int_eq("user->ru_uid == 411", 411, user->ru_uid);
			assert_int_eq("user->ru_gid == 1089", 1089, user->ru_gid);
			assert_str_eq("user->ru_dir is /srv/oper/info", "/srv/oper/info", user->ru_dir);
			break;

		case RES_FILE:
			file_count++;
			file = (struct res_file*)(r->resource);
			assert_not_null("got the first res_file defined", file);
			assert_int_eq("file->rf_mode == 0600", file->rf_mode, 0600);
			assert_str_eq("file->rf_rpath is std/etc-sudoers", "std/etc-sudoers", file->rf_rpath);
			assert_str_eq("file->rf_lpath is /etc/sudoers", "/etc/sudoers", file->rf_lpath);
			break;

		case RES_GROUP:
			group_count++;
			group = (struct res_group*)(r->resource);
			assert_not_null("got the first res_group defined", group);
			assert_int_eq("group->rg_gid == 5454", 5454, group->rg_gid);
			assert_str_eq("group->rg_name is group54", "group54", group->rg_name);
			break;

		default:
			assert_fail("Unknown resource type");
			break;

		}
	}
	assert_int_eq("Only found 1 user",  1, user_count);
	assert_int_eq("Only found 1 file",  1, file_count);
	assert_int_eq("Only found 1 group", 1, group_count);

	policy_free_all(pol);
}

void test_stree_conditional_policy_generation()
{
	struct policy *pol;
	struct stree *root;
	struct hash *facts;

	struct resource *r, *res;
	struct res_user *user;
	struct res_file *file;

	test("stree: conditional stree generates policies based on facts");
	root = conditional_policy();
	assert_not_null("(test sanity) conditional_policy() should return an stree pointer", root);

	test("stree: conditional policy generation for Lucid/2.6");
	facts = facts_for_lucid26();
	pol = policy_generate(root, facts);
	assert_not_null("policy defined successfully from stree", pol);

	assert_policy_has_users( "policy defined has 1 user resource",    pol, 1);
	assert_policy_has_groups("policy defined has no group resources", pol, 0);
	assert_policy_has_files( "policy defined has 1 file resource",    pol, 1);

	res = NULL;
	for_each_resource(r, pol) { if (r->type == RES_FILE) { res = r; break; } }
	assert_not_null("first resource is not NULL", res);
	file = (struct res_file*)(res->resource);
	assert_not_null("got the first res_file defined", file);
	assert_int_eq("file->rf_mode == 0644", file->rf_mode, 0644);
	assert_str_eq("file->rf_rpath is std/2.6.conf", "std/2.6.conf", file->rf_rpath);
	assert_str_eq("file->rf_lpath is snmpd.conf", "snmpd.conf", file->rf_lpath);

	res = NULL;
	for_each_resource(r, pol) { if (r->type == RES_USER) { res = r; break; } }
	assert_not_null("first resource is not NULL", res);
	user = (struct res_user*)(res->resource);
	assert_not_null("got the first res_user defined", user);
	assert_int_eq("user->ru_uid == 20050", 20050, user->ru_uid);
	assert_int_eq("user->ru_gid == 20051", 20051, user->ru_gid);
	assert_str_eq("user->ru_dir is /srv/oper/ubuntu", "/srv/oper/ubuntu", user->ru_dir);

	hash_free(facts);
	policy_free_all(pol);

	test("stree: conditional policy generation for Tikanga/2.4");
	facts = facts_for_tikanga24();
	pol = policy_generate(root, facts);
	assert_not_null("policy defined successfully from stree", pol);

	assert_policy_has_users( "policy defined has no user resources",  pol, 0);
	assert_policy_has_groups("policy defined has no group resources", pol, 0);
	assert_policy_has_files( "policy defined has 1 file resource",    pol, 1);

	res = NULL;
	for_each_resource(r, pol) { if (r->type == RES_FILE) { res = r; break; } }
	assert_not_null("first file resource is not NULL", res);
	file = (struct res_file*)(res->resource);
	assert_not_null("got the first res_file defined", file);
	assert_int_eq("file->rf_mode == 0644", file->rf_mode, 0644);
	assert_str_eq("file->rf_rpath is std/2.4.conf", "std/2.4.conf", file->rf_rpath);
	assert_str_eq("file->rf_lpath is snmpd.conf", "snmpd.conf", file->rf_lpath);

	hash_free(facts);
	policy_free_all(pol);
}

static struct stree* prog_policy()
{
	struct stree *root, *node, *res, *prog;

	root = NODE(PROG, NULL, NULL);
	if (!root) {
		return NULL;
	}

	/* if test.users == "2" */
	node = child_of(root, NODE(IF, "test.users", "2"));
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

struct hash* facts_for_prog1(void)
{
	struct hash *h = hash_new();
	hash_set(h, "test.users", "1");
	return h;
}

struct hash* facts_for_prog2(void)
{
	struct hash *h = hash_new();
	hash_set(h, "test.users", "2");
	return h;
}

void test_stree_prog_policy_generation()
{
	struct policy *pol;
	struct stree *root;
	struct hash *facts;
	unsigned int i;

	struct resource *res, *r;
	struct res_group *group;

	test("stree: prog(resssion) stree generates policies based on facts");
	root = prog_policy();
	assert_not_null("(test sanity) prog_policy should return an stree pointer", root);

	test("stree: prog(ression) policy generation for 1 group");
	facts = facts_for_prog1();
	pol = policy_generate(root, facts);
	assert_not_null("policy defined successfully from stree", pol);
	assert_policy_has_groups("policy defined has 1 group resource", pol, 1);

	res = NULL;
	for_each_resource(r, pol) { if (r->type == RES_GROUP) { res = r; break; } }
	assert_not_null("group resource was found", res);
	group = (struct res_group*)(res->resource);
	assert_not_null("got the first res_group defined", group);
	assert_int_eq("group->rg_gid == 101", 101, group->rg_gid);

	hash_free(facts);
	policy_free_all(pol);

	test("stree: prog(ression) policy generation for 2 groups");
	facts = facts_for_prog2();
	pol = policy_generate(root, facts);
	assert_not_null("policy defined successfully from stree", pol);
	assert_policy_has_groups("policy defined has 2 group resources", pol, 2);

	i = 0;
	for_each_resource(r, pol) {
		if (r->type != RES_GROUP) { continue; }
		group = (struct res_group*)(r->resource);

		switch (i++) {
		case 0:
			assert_not_null("got the first res_group defined", group);
			assert_int_eq("group->rg_gid == 103", 103, group->rg_gid);
			break;
		case 1:
			assert_not_null("got the second res_group defined", group);
			assert_int_eq("group->rg_gid == 104", 104, group->rg_gid);
			break;
		}
	}
	assert_int_eq("Found 2 groups", 2, i);

	hash_free(facts);
	policy_free_all(pol);
}

void test_stree_comparison()
{
	struct stree *a, *b, *c;

	a = static_policy();
	b = static_policy();
	c = conditional_policy();

	test("stree: abstract syntax tree comparison");
	assert_not_null("(test sanity) stree a isn't null", a);
	assert_not_null("(test sanity) stree b isn't null", b);
	assert_not_null("(test sanity) stree c isn't null", c);

	assert_int_eq("stree(a) == stree(b)", 0, stree_compare(a,b));
	assert_int_ne("stree(b) != stree(c)", 0, stree_compare(b,c));
	assert_int_ne("stree(a) != stree(c)", 0, stree_compare(a,c));
}

void test_suite_stree()
{
	MANIFEST = manifest_new();

	test_stree_child_nodes();

	test_stree_static_policy_generation();
	test_stree_conditional_policy_generation();
	test_stree_prog_policy_generation();

	test_stree_comparison();

	manifest_free(MANIFEST);
}
