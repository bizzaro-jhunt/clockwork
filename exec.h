#ifndef EXEC_H
#define EXEC_H

#include "clockwork.h"

/** @file exec.h

  The exec module provides the means to execute a command
  in a sub-process and capture standard output, standard error,
  or both.
 */

/**
  Execute a command, with optional output capture

  Commands will be executed via '/bin/sh -c'

  The \a std_out and \a std_err parameters are return-value
  parameters.  Unless passed as NULL, they will be set to point
  to heap-allocated memory and contain the standard output and
  standard error output, respectively.

  @param  cmd      Command to run.
  @param  std_out  Pointer for capturing standard output.
  @param  std_err  Pointer for capturing standard error.

  @returns 0 on success; non-zero on failure.
 */
int exec_command(const char *cmd, char **std_out, char **std_err);

#endif
