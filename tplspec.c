#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "template.h"
#include "policy.h"
#include "tpl/parser.h"

static const char *OP_NAMES[] = {
	"TNODE_NOOP",
	"TNODE_ECHO",
	"TNODE_VALUE",
	"TNODE_REF",
	"TNODE_IF_EQ",
	"TNODE_FOR",
	NULL
};

/* FIXME: move this into string.o? */
static char* escape_newlines(const char *s)
{
	char *b,   *b1;
	const char *s1;
	size_t n;

	for (s1 = s; *s1; s1++) {
		if (*s1 == '\r' || *s1 == '\n') { n++; }
		n++;
	}

	b = xmalloc(sizeof(char) * (n + 1));
	for (b1 = b; *s; s++) {
		switch (*s) {
			case '\r': *b1++ = '\\'; *b1++ = 'r'; break;
			case '\n': *b1++ = '\\'; *b1++ = 'n'; break;
			default:   *b1++ = *s;
		}
	}
	*b1 = '\0';

	return b;
}

static void traverse(struct tnode *node, unsigned int depth)
{
	char *buf;
	char *d1 = NULL;
	char *d2 = NULL;
	unsigned int i;

	buf = xmalloc(2 * depth + 1);
	memset(buf, ' ', 2 * depth);
	buf[2 * depth] = '\0';

	if (node->d1) { d1 = escape_newlines(node->d1); }
	if (node->d2) { d2 = escape_newlines(node->d2); }

	printf("%s(%u:%s // %s // %s) [%u] 0x%p\n", buf, node->type, OP_NAMES[node->type], d1, d2, node->size, node);
	free(d1); free(d2); free(buf);

	for (i = 0; i < node->size; i++) {
		traverse(node->nodes[i], depth + 1);
	}
}

int main(int argc, char **argv)
{
	struct template *template;
	struct hash *facts = hash_new();
	char *output;

	printf("TPL:SPEC   A Template Compiler\n"
	       "\n"
	       "This program will traverse an abstract syntax tree generated\n"
	       "by the lex/yacc parser allowing you to verify correctness\n"
	       "\n"
	       "Fact values read from standard input are available for expansion.\n"
	       "\n");

	if (argc < 2) {
		fprintf(stderr, "USAGE: %s /path/to/template\n", argv[0]);
		exit(1);
	}

	fact_read(stdin, facts);
	template = template_create(argv[1], facts);
	if (!template) {
		exit(2);
	}

	traverse(template->root, 0);

	printf("\n\n");

	template_add_var(template, "resource.path", "/path/to/resource");

	output = template_render(template);
	printf("TEMPLATE OUTPUT:\n"
	       "----------------------------------------------------------------------\n"
	       "%s"
	       "----------------------------------------------------------------------\n",
	       output);

	return 0;
}
