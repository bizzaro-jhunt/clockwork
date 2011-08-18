#include "../../clockwork.h"
#include "../../prompt.h"

int main(int argc, char **argv)
{
	char *v = prompt("Test string: ");
	fprintf(stderr, "%u:%s:\n", strlen(v), v);
	return 0;
}

