#ifndef REPORT_H
#define REPORT_H

#include "clockwork.h"
#include "list.h"
#include "pack.h"

/**
  Possible results of a single operation.
 */
enum action_result {
	ACTION_SUCCEEDED = 0,
	ACTION_FAILED,
	ACTION_SKIPPED
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
	struct list report;
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

	/** List member node, for stringing multiple report
	    structures together (for a single host / session) */
	struct list rep;
	/** List of all actions carried out for this resource. */
	struct list actions;
};

struct report* report_new(const char *type, const char *key);
void report_free(struct report *report);

int report_action(struct report *report, char *summary, enum action_result result);
void report_print(FILE *io, struct report *report);

char* report_pack(const struct report *report);
struct report* report_unpack(const char *packed);
char* action_pack(const struct action *action);
struct action* action_unpack(const char *packed, struct report *report);


#endif
