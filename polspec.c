#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "fact.h"

/* defined in spec/parser.y */
extern struct ast *spec_result;
extern int yyparse(void);
extern int parse_success;

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
	if (parse_success == 1) {
		return spec_result;
	}
	return NULL;
}

static int get_the_facts(struct list *facts, const char *path)
{
	FILE *io;
	int nfacts;

	io = fopen(path, "r");
	if (!io) {
		fprintf(stderr, "Unable to read facts from %s: ", path);
		perror(NULL);
		return 0;
	}

	nfacts = fact_read(facts, io);
	fclose(io);

	return nfacts;
}

int main(int argc, char **argv)
{
	struct ast *root;
	struct policy *policy;
	struct list facts;

	printf("POL:SPEC   A Policy Specification Compiler\n" \
	       "\n" \
	       "This program will traverse an abstract syntax tree generated\n" \
	       "by the lex/yacc parser allowing you to verify correctness\n" \
	       "\n");

	if (argc != 3) {
		fprintf(stderr, "USAGE: %s /path/to/config /path/to/facts\n", argv[0]);
		exit(1);
	}

	root = parse_file(argv[1]);
	if (!root) {
		exit(2);
	}
	traverse(root, 0);


	list_init(&facts);
	if (get_the_facts(&facts, argv[2]) <= 0) {
		return 2;
	}

	if (root->size > 0) {
		policy = ast_evaluate(root->nodes[0], &facts);
		printf("Policy in packed format:\n" \
		       "------------------------\n%s\n",
		       policy_pack(policy));
	} else {
		fprintf(stderr, "No policy defined...\n");
	}

	return 0;
}
