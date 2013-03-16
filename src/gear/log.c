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

#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>

#include "gear.h"

static int SYSLOG_INITIALIZED = 0;
static int LOG_LEVEL = LOG_LEVEL_INFO;

static const char* LOG_LEVEL_NAMES[] = {
	NULL,
	"NONE",
	"CRITICAL",
	"ERROR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
	"ALL"
};

#define log2syslog(prio,level) do { \
	if (level > LOG_LEVEL) { return; } \
	va_list ap; \
	va_start(ap, format); \
	if (SYSLOG_INITIALIZED) { \
		vsyslog(prio, format, ap); \
	} else { \
		vfprintf(stderr, format, ap); \
		fprintf(stderr, "\n"); \
	} \
	va_end(ap); \
} while (0)

/**
  Initialize the logging subsystem.

  Sets up the logging functions to send messages to syslog,
  with a message prefix of $ident.  For information on how
  $ident is used, see syslog(3).

  The logging subsystem allows other subsystems to generate messages of
  different severities.  The system as a whole can then be configured to
  either pass these messages to the standard syslog daemon, or send them
  to stderr.

  ### Log Level ######################################

  The log level (managed by @log_set) determines which messages
  are processed and which are discarded.  If a log message is generated
  with a severity equal to or higher than the current log level, that
  message will be processed.  Any messages logged with lower severities
  than the current log level will be silently ignored.

  Here's an example:

  <code>
  log_set(LOG_LEVEL_ERROR);
  ERROR("this will get logged");            // processed
  CRITICAL("so will this critical");        // processed
  NOTICE("notices will be discarded");      // IGNORED

  log_set(LOG_LEVEL_DEBUG);
  NOTICE("now, notices will be processed"); // processed

  log_set(LOG_LEVEL_NONE);
  CRITICAL("silent emergency");             // IGNORED
  </code>

  ### Syslog Support #################################

  By default, the logging system "logs" stuff to the standard error stream,
  stderr (UNIX file descriptor 2).  The @log_init function will
  instead redirect all subsequent messages to syslog (pursuant to log level,
  as described above).  All syslog messages are sent through the `DAEMON`
  facility.

  <code>
  log_set(LOG_LEVEL_ALL);
  ERROR("pre-log_init message"); // prints to stderr

  log_init("daemon-name");
  ERROR("post-log_init message"); // sent to syslog
  </code>
 */
void log_init(const char *ident) {
	if (SYSLOG_INITIALIZED == 0) {
		openlog(ident, LOG_CONS | LOG_PID, LOG_DAEMON);
		SYSLOG_INITIALIZED = 1;
	}
}

/**
  Set the current log level to $l.

  The $l parameter must be one of the LOG_LEVEL_* constants:

  + $LOG_LEVEL_CRITICAL - Fatal issues causing immediate termination.
  + $LOG_LEVEL_ERROR    - Non-fatal issues that hinder operation.
  + $LOG_LEVEL_WARNING  - Minor problems that do not hinder operation.
  + $LOG_LEVEL_NOTICE   - Significant, non-problematic conditions.
  + $LOG_LEVEL_INFO     - Informational messages to assist troubleshooting.
  + $LOG_LEVEL_DEBUG    - Informational messages to assist developers.

  The log level can also be set to one of two special values:

  + $LOG_LEVEL_NONE - Discard all messages.
  + $LOG_LEVEL_ALL  - Process all messages.

  The default log level is $LOG_LEVEL_ALL

  Because this function does check parameters before setting log level,
  it may not always pass back the log level passed in.

  For example:

  <code>
  int l;

  // Too low...
  l = log_set(-1)      // l = LOG_LEVEL_NONE
  l = log_set(-999)    // l = LOG_LEVEL_NONE

  // Too high!
  l = log_set(1000)    // l = LOG_LEVEL_ALL
  </code>

  Returns the log level that was actually set.  As state above, this could
  be different from $l, if $l is invalid (i.e. too high or too low).
 */
int log_set(int l)
{
	return LOG_LEVEL = (l > LOG_LEVEL_ALL ? LOG_LEVEL_ALL : (l < LOG_LEVEL_NONE ? LOG_LEVEL_NONE : l));
}

/**
  Get the current log level.

  This function should be used to avoid expending cycles on
  log message creation when the current log level would force
  that message to be discarded.

  Returns the current log level, as a LOG_LEVEL_* constant.
  See #log_set for more details.
 */
int log_level(void)
{
	return LOG_LEVEL;
}


/**
  Get the name of log level $l.

  This function is intended to be used for printing out the current log
  level so that the user know what it is set to.  For example:

  <code>
  int l;

  l = log_set(LOG_LEVEL_INFO);
  INFO("Log level is now %s", log_level_name(l));
  </code>

  Returns an internally-managed constant string containing the name
  of log level $l.  You should not free this string.
 */
const char* log_level_name(int l)
{
	l = (l > LOG_LEVEL_DEBUG ? LOG_LEVEL_DEBUG : (l < LOG_LEVEL_NONE ? LOG_LEVEL_NONE : l));
	return LOG_LEVEL_NAMES[l];
}

/**
  Log A Critical Message.

  Critical messages are for fatal issues that cause immediate process termination.

  This function can be used as a drop-in replacement for calls to printf(3).
 */
void CRITICAL(const char *format, ...)   { log2syslog(LOG_CRIT,    LOG_LEVEL_CRITICAL);  }

/**
  Log An Error Message.

  Error messages are for non-fatal issues that prevent the system from doing something.

  This function can be used as a drop-in replacement for calls to printf(3).
 */
void ERROR(const char *format, ...)      { log2syslog(LOG_ERR,     LOG_LEVEL_ERROR);     }

/**
  Log A Warning Message.

  Warning messages are for minor problems that do not hinder operation.

  This function can be used as a drop-in replacement for calls to printf(3).
 */
void WARNING(const char *format, ...)    { log2syslog(LOG_WARNING, LOG_LEVEL_WARNING);   }

/**
  Log An Important Message.

  This function can be used as a drop-in replacement for calls to printf(3).
 */
void NOTICE(const char *format, ...)     { log2syslog(LOG_NOTICE,  LOG_LEVEL_NOTICE);    }

/**
  Log An Informational Message.

  Informational messages assist system administrators with troubleshooting.

  This function can be used as a drop-in replacement for calls to printf(3).
 */
void INFO(const char *format, ...)       { log2syslog(LOG_INFO,    LOG_LEVEL_INFO);      }

/**
  Log A Debugging Message.

  Debugging messages generally only make sense to the developers, but
  they can be used by end users who need to provide additional information
  in a bug report.

  This function can be used as a drop-in replacement for calls to printf(3).
 */
void DEBUG(const char *format, ...)      { log2syslog(LOG_DEBUG,   LOG_LEVEL_DEBUG);     }

