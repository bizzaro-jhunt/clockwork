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

#include "job.h"

/**
  Create a new Job

  A new job has no reports, and a zero start and end time.

  The pointer returned must be passed to @job_free to
  properly release its allocated memory.

  On success, returns a pointer to a new job.
  On failure, returns NULL.
 */
struct job* job_new(void)
{
	struct job *job;
	job = xmalloc(sizeof(struct job));
	job->duration = 0;
	cw_list_init(&job->reports);
	return job;
}

/**
  Free $job
 */
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

/**
  Start execution timer for $job.

  On success, returns 0.  On failure, returns non-zero.
 */
int job_start(struct job *job)
{
	assert(job); // LCOV_EXCL_LINE
	return gettimeofday(&job->start, NULL);
}

/**
  Stop execution timer for $job.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Add $report to $job.

  On success, returns 0.  On failure, returns non-zero.
 */
int job_add_report(struct job *job, struct report *report)
{
	assert(job); // LCOV_EXCL_LINE
	assert(report); // LCOV_EXCL_LINE

	cw_list_push(&job->reports, &report->l);
	return 0;
}

/**
  Create a new report.

  $type and $key specify the type and key (unique ID) for
  the resource that this report is tied to.

  On success, returns a pointer to a new report object.  This
  pointer must be passed to @report_free to free allocated
  memory.

  On failure, returns NULL.
 */
struct report* report_new(const char *type, const char *key)
{
	struct report *r;

	r = xmalloc(sizeof(struct report));
	r->res_type = xstrdup(type);
	r->res_key  = xstrdup(key);

	r->compliant = 1;
	r->fixed     = 0;

	cw_list_init(&r->actions);
	cw_list_init(&r->l);

	return r;
}

/**
  Free report $r
 */
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

/**
  Add $action to $report.

  On success, returns 0.  On failure, returns non-zero.
 */
int report_add_action(struct report *report, struct action *action)
{
	assert(report); // LCOV_EXCL_LINE
	assert(action); // LCOV_EXCL_LINE

	cw_list_push(&report->actions, &action->l);
	if (action->result == ACTION_FAILED) {
		report->compliant = 0;
	} else {
		report->fixed = 1;
	}
	return 0;
}

/**
 Create an action and add it to $report.

 This function dynamically creates a new action, setting
 $summary and $result, and then adds it to $report.

  The following two lines are equivalent:

  <code>
  report_action(r, "summary", ACTION_SUCCEEDED);
  report_add_action(r, action_new("summary", ACTION_SUCCEEDED));
  </code>

  On success, returns 0.  On failure, returns non-zero.
 */
int report_action(struct report *report, char *summary, enum action_result result)
{
	assert(report); // LCOV_EXCL_LINE
	assert(summary); // LCOV_EXCL_LINE

	struct action *action = action_new(summary, result);;
	return report_add_action(report, action);
}

/**
  Create a new action.

  This function allocates a new action, setting
  $summary and $result.

  On success, returns a pointer to the action.
  On failure, returns NULL.
 */
struct action* action_new(char *summary, enum action_result result)
{
	struct action *action;
	action = xmalloc(sizeof(struct action));
	cw_list_init(&action->l);

	action->summary = summary;
	action->result  = result;

	return action;
}

/**
  Free $action.
 */
void action_free(struct action* action)
{
	if (action) {
		free(action->summary);
	}
	free(action);
}
