#ifndef _LOG_H
#define _LOG_H

/**
  @file log.h

  The logging subsystem allows other subsystems to generate messages of
  different severities.  The system as a whole can then be configured to
  either pass these messages to the standard syslog daemon, or send them
  to stderr.

  <h3>Log Level</h3>

  The log level (managed by log_level) determines which messages
  are processed and which are discarded.  If a log message is generated
  with a severity equal to or higher than the current log level, that
  message will be processed.  Any messages logged with lower severities
  than the current log level will be silently ignored.

  Log levels, in order of severity, are as follows:

  - @b LOG_LEVEL_CRITICAL - For fatal issues that cause immediate process termination.
  - @b LOG_LEVEL_ERROR    - Non-fatal issues that prevent the system from doing something.
  - @b LOG_LEVEL_WARNING  - Minor problems that do not hinder operation.
  - @b LOG_LEVEL_NOTICE   - Significant, non-problematic conditions.
  - @b LOG_LEVEL_INFO     - Informational messages that assist system troubleshooting.
  - @b LOG_LEVEL_DEBUG    - Informational messages that assist developers / bug-hunting.

  The log level can also be set to one of two special values:

  - @b LOG_LEVEL_NONE - Discard all messages.
  - @b LOG_LEVEL_ALL  - Process all messages.

  The default log level is LOG_LEVEL_ALL.

  Here's an example:

  @verbatim
  log_set(LOG_LEVEL_ERROR);
  ERROR("this will get logged");            // processed
  CRITICAL("so will this critical");        // processed
  NOTICE("notices will be discarded");      // IGNORED

  log_set(LOG_LEVEL_DEBUG);
  NOTICE("now, notices will be processed"); // processed

  log_set(LOG_LEVEL_NONE);
  CRITICAL("silent emergency");             // IGNORED
  @endverbatim

  <h3>Logging to syslog</h3>

  By default, the logging system "logs" stuff to the standard error stream,
  stderr (UNIX file descriptor 2).  The <tt>log_init</tt> function will
  instead redirect all subsequent messages to syslog (pursuant to log level,
  as described above).  All syslog messages are sent through the DAEMON
  facility.

  @verbatim
  log_set(LOG_LEVEL_ALL);
  ERROR("pre-log_init message"); // prints to stderr

  log_init("daemon-name");
  ERROR("post-log_init message"); // sent to syslog
  @endverbatim

 */

/* @cond false */
#define LOG_LEVEL_ALL         8
#define LOG_LEVEL_DEBUG       7
#define LOG_LEVEL_INFO        6
#define LOG_LEVEL_NOTICE      5
#define LOG_LEVEL_WARNING     4
#define LOG_LEVEL_ERROR       3
#define LOG_LEVEL_CRITICAL    2
#define LOG_LEVEL_NONE        1
/* @endcond */

/**
  Initialize the logging subsystem, by setting it up to use
  syslog(3) for storing log messages.

  @param  ident    String to prefix all messages with; see syslog(3).
 */
void log_init(const char *ident);

/**
  Set the current log level

  Because this function does check parameters before setting log level,
  it may not always return the log level passed for the \a level parameter.
  For example:

  @verbatim
  int l;

  // Too low...
  l = log_set(-1)      // l = LOG_LEVEL_NONE
  l = log_set(-999)    // l = LOG_LEVEL_NONE

  // Too high!
  l = log_set(1000)    // l = LOG_LEVEL_ALL
  @endverbatim

  @param  level    Desired log verbosity level (a LOG_LEVEL_* constant)

  @returns the actual log level, which may differ if \a level is an unknown
           log level (i.e. too high or too low)
 */
int log_set(int level);

/**
  Return the current log level

  This function should be used to avoid expending cycles on
  log message creation when the current log level would force
  that message to be discarded.

  @returns the current log level, as set by log_set(int)
 */
int log_level(void);

/**
  Return a human-friendly description of a log level

  This function is intended to be used for printing out the current log
  level so that the user know what it is set to.  For example:

  @verbatim
  int l;

  l = log_set(LOG_LEVEL_INFO);
  INFO("Log level is now %s", log_level_name(l));
  @endverbatim

  @param  level  Log level (LOG_LEVEL_*) to retrieve a name for.

  @returns an internal constant string representing the name of the
           given log level.
 */
const char* log_level_name(int level);

/**
  Log critical messages; for fatal issues that cause
  immediate process termination.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void CRITICAL(const char *format, ...);

/**
  Log error messages; for non-fatal issues that prevent
  the system from doing something.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void ERROR(const char *format, ...);

/**
  Log warning messages; for minor problems that do not
  hinder operation.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void WARNING(const char *format, ...);

/**
  Log messages for significant, non-problematic conditions.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void NOTICE(const char *format, ...);

/**
  Log informational messages, to assist system
  administrators with troubleshooting.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void INFO(const char *format, ...);

/**
  Log messages that end users can submit with bug reports.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void DEBUG(const char *format, ...);

#endif
