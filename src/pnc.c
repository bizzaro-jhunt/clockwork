#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <vigor.h>
#include "vm.h"

int main(int argc, char **argv)
{
	log_open("asm", "console");

	byte_t *code;
	size_t len;

	int rc = vm_asm_file("-", &code, &len);
	assert(rc == 0);

	if (write(1, code, len) != len)
		fprintf(stderr, "short write!\n");

	return 0;
}
