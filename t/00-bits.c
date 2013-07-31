#include "test.h"

int main(void) {
	test();

	unsigned int f = 0;
	f |= 00001;
	f |= 00010;
	f |= 00020;
	f |= 00040;
	is_int(f, 071, "001 | 010 | 020 | 040 == 071");

	f |= 0020;
	is_int(f, 071, "setting 020 again still yields 071");

	f &= ~00020;
	is_int(f, 051, "unsetting 020 yields 051");

	f &= ~00020;
	is_int(f, 051, "unsetting 020 again still yields 051");

	done_testing();
}
