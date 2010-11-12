#include <stdio.h>

#include "../../stringlist.h"
#include "../../spec/parser.h"
#include "../../policy.h"

void print_policies(struct manifest *m)
{
	unsigned int i;
	stringlist *names;
	struct stree *root = m->root;

	names = stringlist_new(NULL);
	for (i = 0; i < root->size; i++) {
		stringlist_add(names, root->nodes[i]->data1);
	}

	printf("%s\n", stringlist_join(names, "::"));
}

int main(int argc, char **argv)
{
	struct manifest *manifest;

	if (argc != 2) {
		printf("improper invocation!\n");
		return 1;
	}
	manifest = parse_file(argv[1]);
	if (!manifest) {
		printf("unable to parse_file!\n");
		return 2;
	}

	print_policies(manifest);
	if (manifest->root->size == 0) {
		printf("no policies found!\n");
		return 3;
	}

	return 0;
}

