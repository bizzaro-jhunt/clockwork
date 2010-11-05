#ifndef _FACT_H
#define _FACT_H

#include <stdint.h>
#include <stdio.h>
#include "list.h"

struct fact {
	char *name;
	char *value;

	struct list facts;
};

struct fact* fact_new(const char *name, const char *value);
int fact_init(struct fact* fact, const char *name, const char *value);
void fact_deinit(struct fact *fact);
void fact_free(struct fact *fact);
void fact_free_all(struct list *facts);

struct fact *fact_parse(const char *line);
int fact_read(struct list *facts, FILE *io);

#endif
