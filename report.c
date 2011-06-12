#include "report.h"

struct report* report_new(const char *type, const char *key)
{
	struct report *r;

	r = xmalloc(sizeof(struct report));
	r->res_type = xstrdup(type);
	r->res_key  = xstrdup(key);

	r->compliant = 1;
	r->fixed     = 0;

	list_init(&r->actions);
	list_init(&r->rep);

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
	add = xmalloc(sizeof(struct action));
	list_init(&add->report);

	add->summary = summary;
	add->result  = result;

	list_add_tail(&add->report, &report->actions);

	if (add->result == ACTION_FAILED) {
		report->compliant = 0;
	} else {
		report->fixed = 1;
	}

	return 0;
}

void report_print(FILE *io, struct report *r)
{
	char buf[80];
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

		fprintf(io, " - %-70s%7s\n", buf,
		        (a->result == ACTION_SUCCEEDED ? "done" :
		         (a->result == ACTION_FAILED ? "failed" : "skipped")));
	}
}

#define PACK_FORMAT "aaLL"
char* report_pack(const struct report *report)
{
	return pack("report::", PACK_FORMAT,
	            report->res_type, report->res_key,
	            report->compliant, report->fixed);
}

struct report* report_unpack(const char *packed)
{
	struct report *report = report_new(NULL, NULL);

	if (unpack(packed, "report::", PACK_FORMAT,
	           &report->res_type, &report->res_key,
	           &report->compliant, &report->fixed) != 0) {

		report_free(report);
		return NULL;
	}

	return report;
}
#undef PACK_FORMAT

#define PACK_FORMAT "aL"
char* action_pack(const struct action *action)
{
	return pack("action::", PACK_FORMAT,
	            action->summary, action->result);
}

struct action* action_unpack(const char *packed, struct report *report)
{
	struct action *action;
	action = xmalloc(sizeof(struct action));
	list_init(&action->report);

	if (unpack(packed, "action::", PACK_FORMAT,
	           &action->summary, &action->result) != 0) {

		free(action);
		return NULL;
	}

	list_add_tail(&action->report, &report->actions);
	return action;
}
#undef PACK_FORMAT

