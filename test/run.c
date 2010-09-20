#include "test.h"

extern void test_suite_userdb();

int main(int argc, char **argv)
{
	test_setup(argc, argv);

	test_suite_userdb();
	test_suite_sha1();

	return test_status();
}
