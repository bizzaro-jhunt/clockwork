#ifndef _SPEC_PARSER_H
#define _SPEC_PARSER_H

#include "../ast.h"
#include "../stringlist.h"

typedef struct {
	const char     *fact;
	stringlist     *values;
	unsigned char   affirmative;
	struct ast     *then;
	struct ast     *otherwise;
} parser_branch;

typedef struct {
	const char     *fact;
	const char     *attribute;

	stringlist     *fact_values;
	stringlist     *attr_values;

	const char     *default_value;
} parser_map;

int yylex(void);
void yyerror(const char *);

#endif
