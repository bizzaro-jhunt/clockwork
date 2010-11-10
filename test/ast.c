#include "test.h"
#include "assertions.h"
#include "../ast.h"
#include "../fact.h"

static struct ast* new_child_node(struct ast *parent, unsigned int op, void *data1, void *data2)
{
	struct ast *node;

	node = ast_new(op, data1, data2);
	if (!node) {
		perror("ast_new");
		exit(1);
	}

	if (ast_add_child(parent, node) != 0) {
		fprintf(stderr, "Failed to add node to parent\n");
		exit(2);
	}

	return node;
}


void test_ast_init()
{
	struct ast *ast;

	test("AST: Allocation of new abstract syntax trees");
	ast = ast_new(AST_OP_IF_EQUAL, "data1", "data2");
	assert_not_null("ast_new returns a struct ast pointer", ast);

	assert_str_equals("data1 set properly", "data1", ast->data1);
	assert_str_equals("data2 set properly", "data2", ast->data2);
	assert_int_equals("op set properly", AST_OP_IF_EQUAL, ast->op);

	assert_int_equals("new AST nodes have no references", 0, ast->refs);
	assert_int_equals("new AST nodes have no child nodes", 0, ast->size);
	assert_null("new AST nodes have no child nodes", ast->nodes);

	ast_free(ast);
}

void test_ast_reference_counting()
{
	struct ast *ast;

	test("AST: reference counts");
	ast = ast_new(AST_OP_NOOP, NULL, NULL);
	assert_not_null("ast_new returns a valid pointer", ast);
	assert_int_equals("initially, ref count is 0", 0, ast->refs);
	assert_int_equals("ast_deinit succeeds for ref count of 0", 0, ast_deinit(ast));
	free(ast);

	ast = ast_new(AST_OP_NOOP, NULL, NULL);
	assert_not_null("ast_new (2nd) returns a valid pointer", ast);
	ast->refs = 2;
	assert_int_equals("artificially, ref count is 2", 2, ast->refs);
	assert_int_not_equal("ast_deinit fails for ref count of 2", 0, ast_deinit(ast));
	assert_int_equals("after first deinit, ref count is 1", 1, ast->refs);
	assert_int_not_equal("ast_deinit fails for ref count of 1", 0, ast_deinit(ast));
	assert_int_equals("after second deinit, ref count is 0", 0, ast->refs);
	assert_int_equals("ast_deinit succeeds for ref count of 0", 0, ast_deinit(ast));
	free(ast);
}

void test_ast_child_nodes()
{
	struct ast *root, *child1, *child2;

	test("AST: child node management");
	root = ast_new(AST_OP_DEFINE_POLICY, "policy", NULL);
	assert_not_null("root is a valid ast pointer", root);
	assert_int_equals("initially, root has no children", 0, root->size);
	assert_null("initially, root has no children", root->nodes);

	child1 = ast_new(AST_OP_NOOP, "first", "node");
	assert_not_null("child1 is a valid ast pointer", child1);
	child2 = ast_new(AST_OP_NOOP, "second", "node");
	assert_not_null("child2 is a valid ast pointer", child2);

	assert_int_equals("Adding child1 to root works", 0, ast_add_child(root, child1));
	assert_int_equals("After adding child1, root has one node", 1, root->size);
	assert_true("After adding child1, root->nodes[0] is child1", root->nodes[0] == child1);

	assert_int_equals("Adding child2 to root works", 0, ast_add_child(root, child2));
	assert_int_equals("After adding child2, root has two nodes", 2, root->size);
	assert_true("After adding child2, root->nodes[1] is child2", root->nodes[1] == child2);

	ast_free(child1);
	ast_free(child2);
	ast_free(root);
}

static struct ast* static_policy_ast()
{
	struct ast *root, *node;

	root = ast_new(AST_OP_DEFINE_POLICY, "standard", NULL);
	if (!root) {
		return NULL;
	}

	node = new_child_node(root, AST_OP_DEFINE_RES_FILE, "/etc/sudoers", NULL);
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "owner", "root");
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "group", "root");
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "mode",  "0600");
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "source", "std/etc-sudoers");

	node = new_child_node(root, AST_OP_DEFINE_RES_USER, "user1", NULL);
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "uid", "411");
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "gid", "1089");
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "home", "/srv/oper/info");

	node = new_child_node(root, AST_OP_DEFINE_RES_GROUP, "group54", NULL);
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "gid", "5454");

	return root;
}

void test_ast_static_policy_generation()
{
	struct policy *pol;
	struct ast *root;
	struct list facts;
	unsigned int i;

	struct res_user *user;
	struct res_file *file;
	struct res_group *group;

	test("AST: static AST generates static policy with empty list");
	root = static_policy_ast();
	assert_not_null("(test sanity) static_policy_ast should return an AST pointer", root);

	list_init(&facts);
	pol = ast_evaluate(root, &facts);
	assert_not_null("policy defined successfully from AST", pol);

	assert_true("policy defined has user resources",  !list_empty(&pol->res_users));
	assert_true("policy defined has group resources", !list_empty(&pol->res_groups));
	assert_true("policy defined has file resources",  !list_empty(&pol->res_files));

	i = 0;
	for_each_node(user, &pol->res_users, res) {
		switch (i++) {
		case 0:
			assert_not_null("got the first res_user defined", user);
			assert_int_equals("user->ru_uid == 411", 411, user->ru_uid);
			assert_int_equals("user->ru_gid == 1089", 1089, user->ru_gid);
			assert_str_equals("user->ru_dir is /srv/oper/info", "/srv/oper/info", user->ru_dir);
			break;
		}
	}
	assert_int_equals("Only found 1 user", 1, i);

	i = 0;
	for_each_node(file, &pol->res_files, res) {
		switch (i++) {
		case 0:
			assert_not_null("got the first res_file defined", file);
			assert_int_equals("file->rf_mode == 0600", file->rf_mode, 0600);
			assert_str_equals("file->rf_rpath is std/etc-sudoers", "std/etc-sudoers", file->rf_rpath);
			assert_str_equals("file->rf_lpath is /etc/sudoers", "/etc/sudoers", file->rf_lpath);
			break;
		}
	}
	assert_int_equals("Only found 1 file", 1, i);

	i = 0;
	for_each_node(group, &pol->res_groups, res) {
		switch (i++) {
		case 0:
			assert_not_null("got the first res_group defined", group);
			assert_int_equals("group->rg_gid == 5454", 5454, group->rg_gid);
			assert_str_equals("group->rg_name is group54", "group54", group->rg_name);
			break;
		}
	}
	assert_int_equals("Only found 1 group", 1, i);

	policy_free_all(pol);
	ast_free_all(root);
}

static struct ast* conditional_policy_ast()
{
	struct ast *root, *node, *tmp;

	root = ast_new(AST_OP_DEFINE_POLICY, "standard", NULL);
	if (!root) {
		return NULL;
	}

	node = new_child_node(root, AST_OP_DEFINE_RES_FILE, "snmpd.conf", NULL);
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "owner", "root");
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "group", "root");
	new_child_node(node, AST_OP_SET_ATTRIBUTE, "mode",  "0644");

	tmp = new_child_node(node, AST_OP_IF_EQUAL, "sys.kernel.major", "2.6");
	new_child_node(tmp, AST_OP_SET_ATTRIBUTE, "source", "std/2.6.conf");
	new_child_node(tmp, AST_OP_SET_ATTRIBUTE, "source", "std/2.4.conf");

	node = new_child_node(root, AST_OP_IF_EQUAL, "lsb.distro.codename", "lucid");
	/* if lucid... */
	tmp = new_child_node(node, AST_OP_DEFINE_RES_USER, "ubuntu", NULL);
	new_child_node(tmp, AST_OP_SET_ATTRIBUTE, "uid", "20050");
	new_child_node(tmp, AST_OP_SET_ATTRIBUTE, "gid", "20051");
	new_child_node(tmp, AST_OP_SET_ATTRIBUTE, "home", "/srv/oper/ubuntu");
	/* else */
	/* if karmic... */
	node = new_child_node(node, AST_OP_IF_EQUAL, "lsb.distro.codename", "karmic");
	/* HACK: duplicate the same code as the branch; need better free mechanism */
	tmp = new_child_node(node, AST_OP_DEFINE_RES_USER, "ubuntu", NULL);
	new_child_node(tmp, AST_OP_SET_ATTRIBUTE, "uid", "20050");
	new_child_node(tmp, AST_OP_SET_ATTRIBUTE, "gid", "20051");
	new_child_node(tmp, AST_OP_SET_ATTRIBUTE, "home", "/srv/oper/ubuntu");
	//ast_add_child(node, tmp);
	/* else... */
	new_child_node(node, AST_OP_NOOP, NULL, NULL);

	return root;
}

static void facts_for_lucid26(struct list *facts)
{
	struct fact *fact;

	fact = fact_parse("lsb.distro.codename=lucid\n");
	list_add_tail(&fact->facts, facts);

	fact = fact_parse("sys.kernel.major=2.6\n");
	list_add_tail(&fact->facts, facts);
}

static void facts_for_tikanga24(struct list *facts)
{
	struct fact *fact;

	fact = fact_parse("sys.kernel.major=2.4\n");
	list_add_tail(&fact->facts, facts);

	fact = fact_parse("lsb.distro.codename=tikanga\n");
	list_add_tail(&fact->facts, facts);
}

void test_ast_conditional_policy_generation()
{
	struct policy *pol;
	struct ast *root;
	struct list facts;

	struct res_user *user;
	struct res_file *file;

	test("AST: conditional AST generates policies based on facts");
	root = conditional_policy_ast();
	assert_not_null("(test sanity) conditional_policy_ast should return an AST pointer", root);

	test("AST: conditional policy generation for Lucid/2.6");
	list_init(&facts); facts_for_lucid26(&facts);
	pol = ast_evaluate(root, &facts);
	assert_not_null("policy defined successfully from AST", pol);

	assert_true("policy defined has user resources",  !list_empty(&pol->res_users));
	assert_true("policy defined has no group resources", list_empty(&pol->res_groups));
	assert_true("policy defined has file resources", !list_empty(&pol->res_files));

	file = list_node(pol->res_files.next, struct res_file, res);
	assert_not_null("got the first res_file defined", file);
	assert_int_equals("file->rf_mode == 0644", file->rf_mode, 0644);
	assert_str_equals("file->rf_rpath is std/2.6.conf", "std/2.6.conf", file->rf_rpath);
	assert_str_equals("file->rf_lpath is snmpd.conf", "snmpd.conf", file->rf_lpath);

	user = list_node(pol->res_users.next, struct res_user, res);
	assert_not_null("got the first res_user defined", user);
	assert_int_equals("user->ru_uid == 20050", 20050, user->ru_uid);
	assert_int_equals("user->ru_gid == 20051", 20051, user->ru_gid);
	assert_str_equals("user->ru_dir is /srv/oper/ubuntu", "/srv/oper/ubuntu", user->ru_dir);

	policy_free_all(pol);

	test("AST: conditional policy generation for Tikanga/2.4");
	fact_free_all(&facts); facts_for_tikanga24(&facts);
	pol = ast_evaluate(root, &facts);
	assert_not_null("policy defined successfully from AST", pol);

	assert_true("policy defined has no users resources", list_empty(&pol->res_users));
	assert_true("policy defined has no group resources", list_empty(&pol->res_groups));
	assert_true("policy defined has file resources", !list_empty(&pol->res_files));

	file = list_node(pol->res_files.next, struct res_file, res);
	assert_not_null("got the first res_file defined", file);
	assert_int_equals("file->rf_mode == 0644", file->rf_mode, 0644);
	assert_str_equals("file->rf_rpath is std/2.4.conf", "std/2.4.conf", file->rf_rpath);
	assert_str_equals("file->rf_lpath is snmpd.conf", "snmpd.conf", file->rf_lpath);

	fact_free_all(&facts);
	policy_free_all(pol);
	ast_free_all(root);
}

static struct ast* prog_policy_ast()
{
	struct ast *root, *node, *res, *prog;

	root = ast_new(AST_OP_DEFINE_POLICY, "prog-test", NULL);
	if (!root) {
		return NULL;
	}

	/* if test.users == "2" */
	node = new_child_node(root, AST_OP_IF_EQUAL, "test.users", "2");
	prog = new_child_node(node, AST_OP_PROG, NULL, NULL);

		/* define group103 */
	res = new_child_node(prog, AST_OP_DEFINE_RES_GROUP, "group103", NULL);
	new_child_node(res, AST_OP_SET_ATTRIBUTE, "gid", "103");

		/* define group104 */
	res = new_child_node(prog, AST_OP_DEFINE_RES_GROUP, "group104", NULL);
	new_child_node(res, AST_OP_SET_ATTRIBUTE, "gid", "104");

	/* else */
	prog = new_child_node(node, AST_OP_PROG, NULL, NULL);

		/* define group101 */
	res = new_child_node(prog, AST_OP_DEFINE_RES_GROUP, "group101", NULL);
	new_child_node(res, AST_OP_SET_ATTRIBUTE, "gid", "101");

	return root;
}

void facts_for_prog1(struct list *facts)
{
	struct fact *fact;

	fact = fact_parse("test.users=1\n");
	list_add_tail(&fact->facts, facts);
}

void facts_for_prog2(struct list *facts)
{
	struct fact *fact;

	fact = fact_parse("test.users=2\n");
	list_add_tail(&fact->facts, facts);
}

void test_ast_prog_policy_generation()
{
	struct policy *pol;
	struct ast *root;
	struct list facts;
	unsigned int i;

	struct res_group *group;

	test("AST: prog(resssion) AST generates policies based on facts");
	root = prog_policy_ast();
	assert_not_null("(test sanity) prog_policy_ast should return an AST pointer", root);

	test("AST: prog(ression) policy generation for 1 group");
	list_init(&facts); facts_for_prog1(&facts);
	pol = ast_evaluate(root, &facts);
	assert_not_null("policy defined successfully from AST", pol);
	assert_true("policy defined has group resources", !list_empty(&pol->res_groups));
	i = 0;
	for_each_node(group, &pol->res_groups, res) {
		switch (i++) {
		case 0:
			assert_not_null("got the first res_group defined", group);
			assert_int_equals("group->rg_gid == 101", 101, group->rg_gid);
			break;
		}
	}
	assert_int_equals("Found 1 group", 1, i);
	policy_free_all(pol);

	test("AST: prog(ression) policy generation for 2 groups");
	fact_free_all(&facts); facts_for_prog2(&facts);
	pol = ast_evaluate(root, &facts);
	assert_not_null("policy defined successfully from AST", pol);
	assert_true("policy defined has group resources", !list_empty(&pol->res_groups));
	i = 0;
	for_each_node(group, &pol->res_groups, res) {
		switch (i++) {
		case 0:
			assert_not_null("got the first res_group defined", group);
			assert_int_equals("group->rg_gid == 103", 103, group->rg_gid);
			break;
		case 1:
			assert_not_null("got the second res_group defined", group);
			assert_int_equals("group->rg_gid == 104", 104, group->rg_gid);
			break;
		}
	}
	assert_int_equals("Found 2 groups", 2, i);
	policy_free_all(pol);

	fact_free_all(&facts);
	ast_free_all(root);
}

void test_ast_include_expansion()
{
	struct ast *root;
	struct ast *pol1,  *pol2,  *pol3;
	struct ast *prog1, *prog2, *prog3;
	struct ast *ext1, *ext2;

	test("AST: include expansion");
	root  = ast_new(AST_OP_PROG, NULL, NULL);

	pol1  = new_child_node(root, AST_OP_DEFINE_POLICY, "pol1", NULL);
	prog1 = new_child_node(pol1, AST_OP_PROG, NULL, NULL);

	pol2  = new_child_node(root, AST_OP_DEFINE_POLICY, "pol2", NULL);
	prog2 = new_child_node(pol2, AST_OP_PROG, NULL, NULL);

	pol3  = new_child_node(root, AST_OP_DEFINE_POLICY, "pol3", NULL);
	prog3 = new_child_node(pol3, AST_OP_PROG, NULL, NULL);

	ext1  = new_child_node(prog3, AST_OP_INCLUDE, "pol2", NULL);
	ext2  = new_child_node(prog3, AST_OP_INCLUDE, "pol1", NULL);

	assert_int_equals("Go through 2 expansions", 2, ast_expand_includes(root));

	assert_int_equals("ext1->size should be 1 (prog)", 1, ext1->size);
	assert_ptr("ext1->nodes[0] is prog2", prog2, ext1->nodes[0]);

	assert_int_equals("ext2->size should be 1 (prog)", 1, ext2->size);
	assert_ptr("ext2->nodes[0] is prog1", prog1, ext2->nodes[0]);

	ast_free(root);
	ast_free(pol1); ast_free(prog1);
	ast_free(pol2); ast_free(prog2);
	ast_free(pol3); ast_free(prog3);
	ast_free(ext1); ast_free(ext2);
}

void test_ast_comparison()
{
	struct ast *a, *b, *c;

	a = static_policy_ast();
	b = static_policy_ast();
	c = conditional_policy_ast();

	test("AST: abstract syntax tree comparison");
	assert_not_null("(test sanity) AST a isn't null", a);
	assert_not_null("(test sanity) AST b isn't null", b);
	assert_not_null("(test sanity) AST c isn't null", c);

	assert_int_equals("AST(a) == AST(b)", 0, ast_compare(a,b));
	assert_int_not_equal("AST(b) != AST(c)", 0, ast_compare(b,c));
	assert_int_not_equal("AST(a) != AST(c)", 0, ast_compare(a,c));

	ast_free_all(a);
	ast_free_all(b);
	ast_free_all(c);
}

void test_suite_ast()
{
	test_ast_init();
	test_ast_reference_counting();
	test_ast_child_nodes();

	test_ast_static_policy_generation();
	test_ast_conditional_policy_generation();
	test_ast_prog_policy_generation();
	test_ast_include_expansion();

	test_ast_comparison();
}
