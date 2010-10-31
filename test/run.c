#include "test.h"

extern void test_suite_list();
extern void test_suite_stringlist();

extern void test_suite_userdb();
extern void test_suite_sha1();

extern void test_suite_pack();

extern void test_suite_res_file();
extern void test_suite_res_user();
extern void test_suite_res_group();

extern void test_suite_policy();

extern void test_suite_host_registry();

int main(int argc, char **argv)
{
	test_setup(argc, argv);

	test_suite_list();
	test_suite_stringlist();

	test_suite_userdb();
	test_suite_sha1();

	test_suite_pack();

	test_suite_res_file();
	test_suite_res_user();
	test_suite_res_group();

	test_suite_policy();

	test_suite_host_registry();

	return test_status();
}
