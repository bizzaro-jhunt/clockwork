#include "job.h"
#include "stringlist.h"
#include "pack.h"

struct job* job_new(void)
{
	struct job *job;
	job = xmalloc(sizeof(struct job));
	job->duration = 0;
	list_init(&job->reports);
	return job;
}

int job_start(struct job *job)
{
	assert(job);
	return gettimeofday(&job->start, NULL);
}

int job_end(struct job *job)
{
	assert(job);

	struct timeval diff;

	if (gettimeofday(&job->end, NULL) != 0) {
		return -1;
	}

	timersub(&job->end, &job->start, &diff);
	job->duration = diff.tv_sec * 1000000 + diff.tv_usec;
	return 0;
}

#define JOB_PACK_FORMAT    "LLL"
#define REPORT_PACK_FORMAT "aaLL"
#define ACTION_PACK_FORMAT "aL"
char *job_pack(const struct job *job)
{
	assert(job);

	struct report *report;
	struct action *action;
	char *packed;
	stringlist *pack_list;

	pack_list = stringlist_new(NULL);
	if (!pack_list) {
		return NULL;
	}

	packed = pack("job::", JOB_PACK_FORMAT,
	              job->start.tv_sec, job->end.tv_usec,
	              job->duration);
	if (!packed || stringlist_add(pack_list, packed) != 0) {
		goto pack_failed;
	}
	xfree(packed);

	for_each_report(report, job) {
		packed = pack("report::", REPORT_PACK_FORMAT,
		              report->res_type, report->res_key,
		              report->compliant, report->fixed);
		if (!packed || stringlist_add(pack_list, packed) != 0) {
			goto pack_failed;
		}
		xfree(packed);

		for_each_action(action, report) {
			packed = pack("action::", ACTION_PACK_FORMAT,
			              action->summary, action->result);
			if (!packed || stringlist_add(pack_list, packed) != 0) {
				goto pack_failed;
			}
			xfree(packed);
		}
	}

	packed = stringlist_join(pack_list, "\n");
	stringlist_free(pack_list);
	return packed;

pack_failed:
	xfree(packed);
	stringlist_free(pack_list);
	return NULL;
}

struct job* job_unpack(const char *packed_job)
{
	assert(packed_job);

	struct job *job = NULL;
	struct report *report = NULL;
	struct action *action = NULL;
	stringlist *pack_list;
	char *packed;
	size_t i;

	char *a_summary;
	enum action_result a_result;

	pack_list = stringlist_split(packed_job, strlen(packed_job), "\n");
	if (!pack_list) {
		return NULL;
	}

	if (pack_list->num == 0) {
		goto unpack_failed;
	}

	job = job_new();
	if (!job) {
		goto unpack_failed;
	}

	packed = pack_list->strings[0];
	if (strncmp(packed, "job::", strlen("job::")) != 0) {
		goto unpack_failed;
	}

	if (unpack(packed, "job::", JOB_PACK_FORMAT,
	           &job->start.tv_sec, &job->end.tv_sec,
	           &job->duration) != 0) {
		goto unpack_failed;
	}

	for (i = 1; i < pack_list->num; i++) {
		packed = pack_list->strings[i];
		if (strncmp(packed, "report::", strlen("report::")) == 0) {
			report = report_new(NULL, NULL);
			if (unpack(packed, "report::", REPORT_PACK_FORMAT,
			           &report->res_type, &report->res_key,
			           &report->compliant, &report->fixed) != 0) {
				goto unpack_failed;
			}
			list_add_tail(&report->l, &job->reports);

		} else if (strncmp(packed, "action::", strlen("action::")) == 0) {
			action = xmalloc(sizeof(struct action));
			list_init(&action->l);

			if (unpack(packed, "action::", ACTION_PACK_FORMAT,
			           &a_summary, &a_result) != 0) {
				goto unpack_failed;
			}
			report_action(report, a_summary, a_result);

		} else {
			goto unpack_failed;
		}
	}

	return job;

unpack_failed:
	/* FIXME: clean up allocated memory */
	return NULL;
}

struct report* report_new(const char *type, const char *key)
{
	struct report *r;

	r = xmalloc(sizeof(struct report));
	r->res_type = xstrdup(type);
	r->res_key  = xstrdup(key);

	r->compliant = 1;
	r->fixed     = 0;

	list_init(&r->actions);
	list_init(&r->l);

	return r;
}

void report_free(struct report *r)
{
	struct action *a, *tmp;

	if (r) {
		for_each_node_safe(a, tmp, &r->actions, l) {
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
	list_init(&add->l);

	add->summary = summary;
	add->result  = result;

	list_add_tail(&add->l, &report->actions);

	if (add->result == ACTION_FAILED) {
		report->compliant = 0;
	} else {
		report->fixed = 1;
	}

	return 0;
}

