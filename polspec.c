#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "policy.h"
#include "spec/parser.h"

static const char *OP_NAMES[] = {
	"NOOP",
	"PROG",
	"IF",
	"INCLUDE",
	"ENFORCE",
	"POLICY",
	"HOST",
	"RESOURCE",
	"ATTR",
	NULL
};

static void traverse(struct stree *node, unsigned int depth)
{
	char *buf;
	unsigned int i;

	buf = malloc(2 * depth + 1);
	memset(buf, ' ', 2 * depth);
	buf[2 * depth] = '\0';

	fprintf(stderr, "%s(%u:%s // %s // %s) 0x%p\n", buf, node->op, OP_NAMES[node->op], node->data1, node->data2, node);

	for (i = 0; i < node->size; i++) {
		traverse(node->nodes[i], depth + 1);
	}
}

int main(int argc, char **argv)
{
	struct manifest *manifest;

	printf("POL:SPEC   A Policy Specification Compiler\n" \
	       "\n" \
	       "This program will traverse an abstract syntax tree generated\n" \
	       "by the lex/yacc parser allowing you to verify correctness\n" \
	       "\n");

	if (argc < 2) {
		fprintf(stderr, "USAGE: %s /path/to/config\n", argv[0]);
		exit(1);
	}

	manifest = parse_file(argv[1]);
	if (!manifest) {
		exit(2);
	}

	traverse(manifest->root, 0);

	if (argc > 2) {
		int i;
		struct stree *p;
		for (i = 2; i < argc; i++) {
			printf("Checking policy '%s': ", argv[i]);
			p = hash_lookup(manifest->policies, argv[i]);
			printf("%p\n", p);
		}
	}
	return 0;
}
