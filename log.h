#ifndef _LOG_H
#define _LOG_H

/**
  @file log.h

  The logging subsystem allows other subsystems to generate messages of
  different severities.  The system as a whole can then be configured to
  either pass these messages to the standard syslog daemon, or send them
  to stderr.

 */

/**
  Initialize the logging subsystem, by setting it up to use
  syslog(3) for storing log messages.

  @param  ident    String to prefix all messages with; see syslog(3).
 */
void log_init(const char *ident);

/**
  Print messages helpful to developers.

  This function is always defined, but does nothing unless the macro DEVEL is
  defined.  When enabled, this function prints to stderr, and is essentially
  similar to a call to fprintf.  A trailing "\n" will be added.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void DEVELOPER(const char *format, ...);

/**
  Log debug messages.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void DEBUG(const char *format, ...);

/**
  Log less informational messages.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void INFO(const char *format, ...);

/**
  Log more informational messages.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void NOTICE(const char *format, ...);

/**
  Log warning messages.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void WARNING(const char *format, ...);

/**
  Log error messages.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void ERROR(const char *format, ...);

/**
  Log critical error messages.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void CRITICAL(const char *format, ...);

/**
  Log alert messages.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void ALERT(const char *format, ...);

/**
  Log emergency error messages.

  @param  format    A printf-style format string.
  @param  ...       Values for expansion of \a format.
 */
void EMERGENCY(const char *format, ...);

#endif
