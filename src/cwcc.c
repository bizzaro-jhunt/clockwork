/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

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
	"RESOURCE",
	"ATTR",
	"RESOURCE_ID",
	"DEPENDENCY",
	"HOST",
	"POLICY",

	"EXPR_NOOP",
	"EXPR_VAL",
	"EXPR_FACT",
	"EXPR_REGEX",

	"EXPR_AND",
	"EXPR_OR",
	"EXPR_NOT",
	"EXPR_EQ",
	"EXPR_MATCH",
	NULL
};
#define MAX_OP EXPR_MATCH

#define REDUNDANT_NODE(n) (((n)->op == PROG || (n)->op == NOOP) && (n)->size == 1)

static int redundant_nodes(struct stree *node)
{
	int i;
	int n = 0;

	if (REDUNDANT_NODE(node)) { n++; }

	for (i = 0; i < node->size; i++) {
		n += redundant_nodes(node->nodes[i]);
	}

	return n;
}

static int count_nodes(struct stree *node)
{
	int i;
	int n = 1;

	if (node->size == 0) { return 1; }
	for (i = 0; i < node->size; i++) {
		n += count_nodes(node->nodes[i]);
	}
	return n;
}

static int size_used_by_attr_names(struct manifest *m)
{
	int i, n = 0;
	struct stree *node;

	for (i = 0; i < m->nodes_len; i++) {
		node = m->nodes[i];
		if (node && node->op == ATTR) {
			n += (strlen(node->data1) + 1) * sizeof(char);
		}
	}

	return n;
}

static int size_used_by_data1(struct manifest *m)
{
	int i, n = 0;
	struct stree *node;

	for (i = 0; i < m->nodes_len; i++) {
		node = m->nodes[i];
		if (node && node->data1) {
			n += (strlen(node->data1) + 1) * sizeof(char);
		}
	}

	return n;
}

static int stree_memory_usage(struct stree *n)
{
	int s = 0;

	if (n) {
		s += sizeof(struct stree);
		if (n->data1) { s += (strlen(n->data1) + 1) * sizeof(n->data1); }
		if (n->data2) { s += (strlen(n->data2) + 1) * sizeof(n->data2); }
		s += sizeof(struct stree*) * n->size;
	}
	return s;
}

static int manifest_memory_usage(struct manifest *m)
{
	int i, s = 0;

	for (i = 0; i < m->nodes_len; i++) {
		s += stree_memory_usage(m->nodes[i]);
	}
	return s;
}

static void traverse(struct stree *node, unsigned int depth)
{
	char *buf;
	unsigned int i;

	buf = cw_alloc(2 * depth + 1);
	memset(buf, ' ', 2 * depth);
	buf[2 * depth] = '\0';

	int d1_empty = node->data1 && *node->data1 == '\0';
	int d2_empty = node->data2 && *node->data2 == '\0';

	printf("%s%s(%u) [%s:%s]\n", buf,
		node->op > MAX_OP ? "UNKNOWN" : OP_NAMES[node->op],
		node->op,
		d1_empty ? "(empty)" : node->data1,
		d2_empty ? "(empty)" : node->data2);
	free(buf);

	for (i = 0; i < node->size; i++) {
		traverse(node->nodes[i], depth + 1);
	}
}

int main(int argc, char **argv)
{
	struct manifest *manifest;
	int redundant, count, mem;

	printf("cwcc - A Clockwork Compiler\n" \
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

	printf("\n");

	count = count_nodes(manifest->root);
	redundant = redundant_nodes(manifest->root);
	mem = manifest_memory_usage(manifest);

	printf("Total Nodes: %u\n", count);
	printf("Redundant:   %u\n", redundant);
	printf("%% Wasted:    %0.2f%%\n", redundant * 100. / count);
	printf("Memory Used: %ub\n", mem);
	printf("\n");

	mem = size_used_by_attr_names(manifest);
	printf("Attr Name Mem Usage: %ib\n", mem);
	mem = size_used_by_data1(manifest);
	printf("Tot Data1 Mem Usage: %ib\n", mem);
	printf("\n");

	if (argc > 2) {
		int i;
		struct stree *p;
		for (i = 2; i < argc; i++) {
			printf("Checking policy '%s': ", argv[i]);
			p = cw_hash_get(manifest->policies, argv[i]);
			printf("%p\n", p);
		}
	}

	manifest_free(manifest);
	return 0;
}
