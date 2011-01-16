#include "report.h"

struct report* report_new(const char *type, const char *key)
{
	struct report *r;

	r = malloc(sizeof(struct report));
	if (r) {
		r->res_type = strdup(type);
		r->res_key  = strdup(key);

		r->compliant = 1;
		r->fixed     = 0;

		list_init(&r->actions);
		list_init(&r->rep);
	}

	return r;
}

void report_free(struct report *r)
{
	struct action *a, *tmp;

	if (r) {
		for_each_node_safe(a, tmp, &r->actions, report) {
			free(a->summary);
			free(a);
		}

		free(r->res_type);
		free(r->res_key);
	}

	free(r);
}

int report_action(struct report *report, char *summary, enum action_result result)
{
	assert(report);
	assert(summary);

	struct action *add;
	add = malloc(sizeof(struct action)); /* FIXME: check malloc return value */
	list_init(&add->report);

	add->summary = summary;
	add->result  = result;

	list_add_tail(&add->report, &report->actions);

	if (add->result == ACTION_SUCCEEDED) {
		report->fixed = 1;
	} else {
		report->compliant = 0;
	}

	return 0;
}

void report_print(FILE *io, struct report *r)
{
	char buf[80];
	//char buf[67];
	struct action *a;

	if (snprintf(buf, 67, "%s: %s", r->res_type, r->res_key) > 67) {
		memcpy(buf + 67 - 1 - 3, "...", 3);
	}
	buf[66] = '\0';

	fprintf(io, "%-66s%14s\n", buf, (r->compliant ? (r->fixed ? "FIXED": "OK") : "NON-COMPLIANT"));

	for_each_node(a, &r->actions, report) {
		memcpy(buf, a->summary, 70);
		buf[70] = '\0';

		if (strlen(a->summary) > 70) {
			memcpy(buf + 71 - 1 - 3, "...", 3);
		}

		fprintf(io, " - %-70s%7s\n", buf, (a->result == ACTION_SUCCEEDED ? "done" : (a->result == ACTION_FAILED ? "failed" : "skipped")));
//		action_print(io, a);
	}
}
