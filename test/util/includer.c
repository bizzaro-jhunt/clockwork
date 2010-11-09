#include <stdio.h>

#include "../../ast.h"
#include "../../stringlist.h"
#include "../../spec/parser.h"

void print_policies(struct ast *root)
{
	unsigned int i;
	stringlist *names;

	names = stringlist_new(NULL);
	for (i = 0; i < root->size; i++) {
		stringlist_add(names, root->nodes[i]->data1);
	}

	printf("%s\n", stringlist_join(names, "::"));
}

int main(int argc, char **argv)
{
	struct ast *root;

	if (argc != 2) {
		printf("improper invocation!\n");
		return 1;
	}
	root = parse_file(argv[1]);
	if (!root) {
		printf("unable to parse_file!\n");
		return 2;
	}

	print_policies(root);
	if (root->size == 0) {
		printf("no policies found!\n");
		return 3;
	}

	return 0;
}

