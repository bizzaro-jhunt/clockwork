#include <stdio.h>

#include "sha1.h"
#include "res_file.h"
#include "res_user.h"

#define SIZEOF(x,pad) printf("sizeof(" #x "): " pad "%u\n", sizeof(x))

int main(int argc, char **arg)
{
	SIZEOF(char, "           ");
	SIZEOF(short, "          ");
	SIZEOF(int, "            ");
	SIZEOF(long, "           ");
	SIZEOF(long long, "      ");
	SIZEOF(sha1, "           ");
	SIZEOF(struct res_file, "");
	SIZEOF(struct res_user, "");

	return 0;
}
