/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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
