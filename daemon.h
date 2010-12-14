#ifndef _DAEMON_H
#define _DAEMON_H

#include <sys/types.h>

/** @file daemon.h

  This file defines a structure for representing a daemonized
  process, and the daemonize routine for becoming one.

 */

/**
  Parameters for daemonization.
 */
struct daemon {
	/** Name of the daemon process */
	const char *name;

	/** Absolute path to a lock file for this daemon.
	    If NULL, no lock file will be created. */
	const char *lock_file;

	/** Absolute path to a PID file for this daemon.
	    If NULL, no PID file will be created. */
	const char *pid_file;
};

/**
  Daemonize the current process, by forking into the background,
  closing stdin, stdout and stderr, and detaching from the
  controlling terminal.

  The \a d parameter allows the caller to pass in parameters for
  daemonization, including a PID file for storing the daemon's
  process ID, and a lock file for ensuring only one instance of
  the daemon is active at one time.

  @param  d    Daemon parameters.

  @returns to the caller if daemonization succeeded.  The parent
           process exits with a status code of 0.  If daemonization
           fails, this call will not return, and a the current
           process will exit non-zero.
 */
void daemonize(const struct daemon *d);

#endif
