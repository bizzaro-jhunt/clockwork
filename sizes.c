#include <stdio.h>
#include <sys/types.h>

#include "sha1.h"
#include "res_file.h"
#include "res_group.h"
#include "res_user.h"

#define SIZEOF(x,pad) printf("sizeof(" #x "): " pad "%u\n", sizeof(x))

int main(int argc, char **arg)
{
	SIZEOF(char, "           ");
	SIZEOF(short, "          ");
	SIZEOF(int, "            ");
	SIZEOF(long, "           ");
	SIZEOF(long long, "      ");
	SIZEOF(void*, "          ");
	printf("\n");
	SIZEOF(uid_t, "          ");
	SIZEOF(gid_t, "          ");
	SIZEOF(mode_t, "         ");
	printf("\n");
	SIZEOF(sha1, "           ");
	printf("\n");
	SIZEOF(struct res_file, "   ");
	SIZEOF(struct res_group, "  ");
	SIZEOF(struct res_user, "   ");

	return 0;
}
