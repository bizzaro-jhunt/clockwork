#ifndef JOB_H
#define JOB_H

#include "clockwork.h"
#include <sys/time.h>
#include "list.h"

/** @file job.h

  This module provides structures and functions for dealing with
  invidual Clockwork agent runs.  These data types are mapped
  directly into the reporting database for job run analysis.

 */

/**
  Iterate over a job's reports

  @param  r    Report (pointer) to use as an iterator
  @param  job  Job
 */
#define for_each_report(r,job) for_each_node((r), &((job)->reports), l)

/**
  Iterate (safely) over a job's reports

  This macro works just like for_each_report, except that the
  report \a r can be removed safely from the list without causing
  strange behavior.

  @param  r    Report (pointer) to use as an iterator
  @param  t    Report (pointer) for temporary storage
  @param  job  Job
 */
#define for_each_report_safe(r,t,job) for_each_node_safe((r), (t), &((job)->reports), l)

/** Iterate over a report's actions

  @param  a    Action (pointer) to use as an iterator
  @param  rpt  Report
 */
#define for_each_action(a,rpt) for_each_node((a), &((rpt)->actions), l)

/** Iterate (safely) over a report's actions

  This macro works just like for_each_action, except that the
  action \a a can be removed safely from the list without causing
  strange behavior.

  @param  a    Action (pointer) to use as an iterator
  @param  t    Action (pointer) for temporary storage
  @param  rpt  Report
 */
#define for_each_action_safe(a,t,rpt) for_each_node_safe((a), (t), &((rpt)->actions), l)

/** Possible results of an action. */
enum action_result {
	ACTION_SUCCEEDED = 0,
	ACTION_FAILED,
	ACTION_SKIPPED
};

/**
  A single execution of the Clockwork agent

  The job structure contains timing data (when did the job start?
  when did it end?) and a full report of everything that was
  evaluated, what was changed to enforce policy, and the results
  of each action.
 */
struct job {
	/** Time (microsecond resolution) that this job started. */
	struct timeval start;

	/** Time (microsecond resolution) that this job ended. */
	struct timeval end;

	/** How long the job ran for (in microseconds). */
	unsigned long duration;

	/** Reports, one for each resource evaluated during the run. */
	struct list reports;
};

/**
  Configuration Change Report

  This structure contains a record of all operations
  taken out a single resource for a given configuration
  change run.
 */
struct report {
	/** Type of resource represented */
	char *res_type;
	/** Unique identifier for the represented resource */
	char *res_key;

	/** Is the resource compliant with policy? */
	int compliant;
	/** Was the resource fixed on this run? */
	int fixed;

	/** List of all actions carried out for this resource. */
	struct list actions;

	/** List member node, for stringing multiple report
	    structures together under a single job */
	struct list l;
};

/**
  A configuration change operation.
 */
struct action {
	/** Human-friendly description of the operation */
	char *summary;
	/** Result of the operation */
	enum action_result result;

	/** List member node, for associating multiple actions
	    to a single report structure.  @see struct report */
	struct list l;
};

/**
  Allocate and initialize a new job structure

  A new job has no reports, and a zero start and end time.

  @note the pointer returned by this function must be passed to
        job_free in order to release memory used by the structure.

  @returns a heap-allocated job structure, or NULL on failure.
 */
struct job* job_new(void);

/**
  Free a job structure

  @param  job    Job to free.
 */
void job_free(struct job *job);

/**
  Start job execution timer.

  After a successful call to job_start, the job::start
  timeval member will have a non-zero value.

  @param  job   Job

  @returns 0 on success, non-zero on failure.
 */
int job_start(struct job *job);

/**
  Stop job execution timer.

  After a successful call to job_end, the job::end
  timeval member will have a non-zero value.

  @param  job   Job

  @returns 0 on success, non-zero on failure.
 */
int job_end(struct job *job);

/**
  Add a report to a job

  @param  job     Job to add \a report to.
  @param  report  Report to add to \a job.

  @returns 0 on success; non-zero on failure.
 */
int job_add_report(struct job *job, struct report *report);

/**
  Serialize a job into a string representation.

  The string returned by this function should be free by the caller,
  using the standard free(3) standard library function.

  The final serialized form can be passed to job_unpack to get
  the original job structure back.

  @param  job    Job to serialize.

  @returns a heap-allocated string containing the packed representation
           of the given job structure and its reports and actions.
 */
char* job_pack(const struct job *job);

/**
  Unserialize a job from its string representation.

  @note the pointer returned by this function should be passed to
        job_free to reclaim its memory.

  @param  packed    Packed (serialized) job representation.

  @returns a heap-allocated job structure, based on the information
           found in the \a packed string.
 */
struct job* job_unpack(const char *packed);

/**
  Allocate and initialize a new report object.

  @note the pointer returned by this function should be passed to
        report_free to reclaim its memory.

  @param  type   Type of the resource this report represents.
  @param  key    Resource key this report represents.

  @returns a pointer to a heap-allocated report object.
 */
struct report* report_new(const char *type, const char *key);

/**
  Free a report object.

  @param  report    Report to free.
 */
void report_free(struct report *report);

/**
  Add an action object to a report

  @param  report   Report to add \a action to.
  @param  action   Action to add to \a report.

  @returns 0 on success, non-zero on failure.
 */
int report_add_action(struct report *report, struct action *action);

/**
  Create and add an action to a report.

  The following two lines are equivalent:

  @verbatim
  report_action(r, "summary", ACTION_SUCCEEDED);
  report_add_action(r, action_new("summary", ACTION_SUCCEEDED));
  @endverbatim

  @param  report   Report to add a new action to.
  @param  summary  Summary of the action.
  @param  result   Result of the action.

  @returns 0 on success, non-zero on failure.
 */
int report_action(struct report *report, char *summary, enum action_result result);

/**
  Allocate and initialize a new action.

  @note the pointer returned by this function must be passed
        to action_free to reclaim its memory.

  @param  summary   Summary of the action.
  @param  result    Result of the action.

  @returns a pointer to a new action, or NULL on failure.
 */
struct action* action_new(char *summary, enum action_result result);

/**
  Free an action.

  @param  action   Action to free.
 */
void action_free(struct action* action);

#endif
