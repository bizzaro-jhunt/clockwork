#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

/* defined in spec/parser.y */
extern struct ast *spec_result;
extern int yyparse(void);

/* defined in spec/lexer.l */
extern FILE *yyin;

static const char *OP_NAMES[] = {
	"NOOP",
	"PROG",
	"IF_EQUAL",
	"IF_NOT_EQUAL",
	"DEFINE_POLICY",
	"DEFINE_RES_USER",
	"DEFINE_RES_GROUP",
	"DEFINE_RES_FILE",
	"SET_ATTRIBUTE",
	NULL
};

static void traverse(struct ast *node, unsigned int depth)
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

static struct ast* parse_file(const char *path)
{
	FILE *io;

	io = fopen(path, "r");
	if (!io) {
		fprintf(stderr, "Unable to parse %s: ", path);
		perror(NULL);
		return NULL;
	}

	yyin = io;
	yyparse();
	return spec_result;
}

int main(int argc, char **argv)
{
	struct ast *root;

	printf("POL:SPEC   A Policy Specification Compiler\n" \
	       "\n" \
	       "This program will traverse an abstract syntax tree generated\n" \
	       "by the lex/yacc parser allowing you to verify correctness\n" \
	       "\n");

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s /path/to/file\n", argv[0]);
		exit(1);
	}

	root = parse_file(argv[1]);
	traverse(root, 0);

	return 0;
}
