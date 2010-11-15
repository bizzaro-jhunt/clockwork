#include <stdio.h>
#include "sha1.h"

#define FATAL "sha1sum: %s"
#define ERR_BUFSIZ 255

int main(int argc, char **argv)
{
	sha1 cksum;
	char err[ERR_BUFSIZ + 1];

	while (*++argv) {
		if (sha1_file(*argv, &cksum) == -1) {
			snprintf(err, ERR_BUFSIZ, FATAL, *argv);
			perror(err);
		} else {
			printf("%s  %s\n", cksum.hex, *argv);
		}
	}

	return 0;
}
