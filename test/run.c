#include "test.h"

extern void test_suite_list();
extern void test_suite_stringlist();
extern void test_suite_hash();

extern void test_suite_userdb();
extern void test_suite_sha1();

extern void test_suite_pack();

extern void test_suite_resources();
extern void test_suite_policy();

extern void test_suite_fact();
extern void test_suite_stree();

extern void test_suite_cert();

typedef void (*test_suite)(void);
static void run_test_suite(const char *run, const char *name, test_suite ts)
{
	if (!run || strcmp(run, name) == 0) { (*ts)(); }
}
#define RUN_TESTS(x) run_test_suite(to_run,#x, test_suite_ ## x)

int main(int argc, char **argv)
{
	const char *to_run;

	test_setup(argc, argv);
	to_run = *(++argv);

	RUN_TESTS(list);
	RUN_TESTS(stringlist);
	RUN_TESTS(hash);

	RUN_TESTS(userdb);
	RUN_TESTS(sha1);

	RUN_TESTS(pack);

	RUN_TESTS(resources);
	RUN_TESTS(policy);

	RUN_TESTS(fact);
	RUN_TESTS(stree);

	RUN_TESTS(cert);

	return test_status();
}
