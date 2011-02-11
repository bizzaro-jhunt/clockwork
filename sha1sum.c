#include <stdio.h>
#include "sha1.h"
#include "mem.h"

int main(int argc, char **argv)
{
	sha1 cksum;
	char *err;

	while (*++argv) {
		if (sha1_file(*argv, &cksum) == -1) {
			err = string("sha1sum: %s", *argv);
			perror(err);
			free(err);
		} else {
			printf("%s %s\n", cksum.hex, *argv);
		}
	}

	return 0;
}
