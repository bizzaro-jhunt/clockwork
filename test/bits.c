#include "test.h"
#include "assertions.h"

void test_basic_twiddles()
{
	unsigned int flags = 0;

	flags |= 00001;
	flags |= 00010;
	flags |= 00020;
	flags |= 00040;
	assert_int_eq("001 | 010 | 020 | 040 == 071", 071, flags);

	flags |= 0020;
	assert_int_eq("setting 020 again still yields 071", 071, flags);

	flags &= ~00020;
	assert_int_eq("unsetting 020 yields 051", 051, flags);

	flags &= ~00020;
	assert_int_eq("unsetting 020 again still yields 051", 051, flags);
}

void test_suite_bits()
{
	test_basic_twiddles();
}
