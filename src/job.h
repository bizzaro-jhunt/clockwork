/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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

#ifndef JOB_H
#define JOB_H

#include "clockwork.h"
#include <sys/time.h>

/* Iterate over job's reports */
#define for_each_report(r,job) for_each_node((r), &((job)->reports), l)

/* Iterate (safely) over a job's reports */
#define for_each_report_safe(r,t,job) for_each_node_safe((r), (t), &((job)->reports), l)

/* Iterate over a report's actions */
#define for_each_action(a,rpt) for_each_node((a), &((rpt)->actions), l)

/* Iterate (safely) over a report's actions */
#define for_each_action_safe(a,t,rpt) for_each_node_safe((a), (t), &((rpt)->actions), l)

/* Possible results of an action. */
enum action_result {
	ACTION_SUCCEEDED = 0,
	ACTION_FAILED,
	ACTION_SKIPPED
};

/**
  A single execution of the Clockwork agent

  The job structure contains timing data (when it started, when
  it ended), a full report of everything that was evaluated, what
  was changed to enforce policy, and the results of each action.
 */
struct job {
	struct timeval start;    /* when the job started */
	struct timeval end;      /* when the job ended */

	unsigned long  duration; /* job duration, in microseconds */
	struct list    reports;  /* Reports for each resource eval'd */
};

/**
  Configuration Change Report

  This structure contains a record of all operations
  taken out a single resource for a given configuration
  change run.
 */
struct report {
	char *res_type;      /* type of resource */
	char *res_key;       /* unique resource ID */

	unsigned char compliant; /* is the resource compliant? */
	unsigned char fixed;     /* did it have to be fixed? */

	struct list actions; /* all actions carried out */
	struct list l;       /* list member node, for struct job */
};

/**
  A configuration change operation.
 */
struct action {
	char              *summary; /* description of the action */
	enum action_result result;  /* result of the action */

	struct list l;  /* list member node, for struct report */
};

struct job* job_new(void);
void job_free(struct job *job);

int job_start(struct job *job);
int job_end(struct job *job);

int job_add_report(struct job *job, struct report *report);
char* job_pack(const struct job *job);
struct job* job_unpack(const char *packed);

struct report* report_new(const char *type, const char *key);
void report_free(struct report *report);
int report_add_action(struct report *report, struct action *action);
int report_action(struct report *report, char *summary, enum action_result result);

struct action* action_new(char *summary, enum action_result result);
void action_free(struct action* action);

#endif
