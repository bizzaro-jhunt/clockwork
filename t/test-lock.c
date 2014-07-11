#include "../src/cw.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("no lock specified!\n");
		exit(1);
	}

	int flags = getenv("TEST_LOCK_FALGS")
	          ? atoi(getenv("TEST_LOCK_FLAGS"))
	          : 0;

	cw_lock_t lock;
	cw_lock_init(&lock, argv[1]);
	if (cw_lock(&lock, flags) == 0) {
		printf("acquired %s\n", lock.path);
		exit(0);
	}

	printf("FAILED: %s in use by %s\n", lock.path, cw_lock_info(&lock));
	exit(1);
}
