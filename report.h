#ifndef REPORT_H
#define REPORT_H

#include "clockwork.h"
#include "list.h"

enum action_result {
	ACTION_SUCCEEDED,
	ACTION_FAILED,
	ACTION_SKIPPED
};

struct action {
	char *summary;
	enum action_result result;

	struct list report;
};

struct report {
	char *res_type;
	char *res_key;

	int compliant;
	int fixed;

	struct list rep;
	struct list actions;
};

struct report* report_new(const char *type, const char *key);
void report_free(struct report *report);

int report_action(struct report *report, char *summary, enum action_result result);
void report_print(FILE *io, struct report *report);


#endif
