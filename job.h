#ifndef JOB_H
#define JOB_H

#include "clockwork.h"
#include <sys/time.h>
#include "list.h"

#define for_each_report(r,job) for_each_node((r), &((job)->reports), l)
#define for_each_action(a,rpt) for_each_node((a), &((rpt)->actions), l)

enum action_result {
	ACTION_SUCCEEDED = 0,
	ACTION_FAILED,
	ACTION_SKIPPED
};

struct job {
	struct timeval start;
	struct timeval end;
	unsigned long duration;

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

struct job* job_new(void);
int job_start(struct job *job);
int job_end(struct job *job);
char* job_pack(const struct job *job);
struct job* job_unpack(const char *packed);

struct report* report_new(const char *type, const char *key);
void report_free(struct report *report);
int report_action(struct report *report, char *summary, enum action_result result);

#endif
