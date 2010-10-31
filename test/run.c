#include "test.h"

extern void test_suite_userdb();

int main(int argc, char **argv)
{
	test_setup(argc, argv);

	test_suite_list();
	test_suite_stringlist();

	test_suite_userdb();
	test_suite_sha1();

	test_suite_serialize();
	test_suite_pack();

	test_suite_res_file();
	test_suite_res_user();
	test_suite_res_group();

	test_suite_policy();

	return test_status();
}
