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

void job_free(struct job *job)
{
	struct report *report, *rtmp;

	if (job) {
		for_each_report_safe(report, rtmp, job) {
			report_free(report);
		}
		free(job);
	}
}

int job_start(struct job *job)
{
	assert(job); // LCOV_EXCL_LINE
	return gettimeofday(&job->start, NULL);
}

int job_end(struct job *job)
{
	assert(job); // LCOV_EXCL_LINE

	struct timeval diff;

	if (gettimeofday(&job->end, NULL) != 0) {
		return -1;
	}

	timersub(&job->end, &job->start, &diff);
	job->duration = diff.tv_sec * 1000000 + diff.tv_usec;
	return 0;
}

int job_add_report(struct job *job, struct report *report)
{
	assert(job); // LCOV_EXCL_LINE
	assert(report); // LCOV_EXCL_LINE

	list_add_tail(&report->l, &job->reports);
	return 0;
}

#define JOB_PACK_FORMAT    "LLL"
#define REPORT_PACK_FORMAT "aaLL"
#define ACTION_PACK_FORMAT "aL"
char* job_pack(const struct job *job)
{
	assert(job); // LCOV_EXCL_LINE

	struct report *report = NULL;
	struct action *action = NULL;
	char *packed;
	stringlist *pack_list;

	pack_list = stringlist_new(NULL);
	if (!pack_list) {
		return NULL;
	}

	packed = pack("job::", JOB_PACK_FORMAT,
	              job->start.tv_sec, job->end.tv_sec, job->duration);
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
	assert(packed_job); // LCOV_EXCL_LINE

	struct job *job = NULL;
	struct report *report = NULL;
	struct action *action = NULL;

	stringlist *pack_list;
	char *packed;
	size_t i;

	pack_list = stringlist_split(packed_job, strlen(packed_job), "\n", SPLIT_GREEDY);
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
			job_add_report(job, report);

		} else if (strncmp(packed, "action::", strlen("action::")) == 0) {
			action = action_new(NULL, ACTION_SKIPPED);
			if (unpack(packed, "action::", ACTION_PACK_FORMAT,
			           &action->summary, &action->result) != 0) {
				goto unpack_failed;
			}
			report_add_action(report, action);

		} else {
			goto unpack_failed;
		}
	}

	stringlist_free(pack_list);
	return job;

unpack_failed:
	stringlist_free(pack_list);
	job_free(job);
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
		for_each_action_safe(a, tmp, r) {
			action_free(a);
		}

		free(r->res_type);
		free(r->res_key);
	}

	free(r);
}

int report_add_action(struct report *report, struct action *action)
{
	assert(report); // LCOV_EXCL_LINE
	assert(action); // LCOV_EXCL_LINE

	list_add_tail(&action->l, &report->actions);
	if (action->result == ACTION_FAILED) {
		report->compliant = 0;
	} else {
		report->fixed = 1;
	}
	return 0;
}

int report_action(struct report *report, char *summary, enum action_result result)
{
	assert(report); // LCOV_EXCL_LINE
	assert(summary); // LCOV_EXCL_LINE

	struct action *action = action_new(summary, result);;
	return report_add_action(report, action);
}

struct action* action_new(char *summary, enum action_result result)
{
	struct action *action;
	action = xmalloc(sizeof(struct action));
	list_init(&action->l);

	action->summary = summary;
	action->result  = result;

	return action;
}

void action_free(struct action* action)
{
	if (action) {
		free(action->summary);
	}
	free(action);
}
