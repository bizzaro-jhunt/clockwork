#include "test.h"

int main(int argc, char **argv)
{
	int run_active;

	TEST_SUITE(bits);
	TEST_SUITE(mem);
	TEST_SUITE(list);
	TEST_SUITE(stringlist);
	TEST_SUITE(hash);
	TEST_SUITE(userdb);
	TEST_SUITE(sha1);
	TEST_SUITE(pack);
	TEST_SUITE(resource);
	TEST_SUITE(resources);
	TEST_SUITE(res_user);
	TEST_SUITE(res_group);
	TEST_SUITE(res_file);
	TEST_SUITE(res_package);
	TEST_SUITE(res_service);
	TEST_SUITE(res_host);
	TEST_SUITE(res_sysctl);
	TEST_SUITE(res_dir);
	TEST_SUITE(policy);
	TEST_SUITE(fact);
	TEST_SUITE(stree);
	TEST_SUITE(cert);
	TEST_SUITE(string);
	TEST_SUITE(job);
	TEST_SUITE(proto);
	TEST_SUITE(template);
	TEST_SUITE(path);

	test_setup(argc, argv);
	run_active = 0;
	while (*(++argv)) {
		run_active += activate_test(*argv);
	}

	(run_active ? run_active_tests() : run_all_tests());
	teardown_test_suites();
	return test_status();
}