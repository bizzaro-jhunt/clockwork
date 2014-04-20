/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#include "exec.h"

#include <sys/wait.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Maximum size of the output buffers for exec_command */
#define EXEC_OUTPUT_MAX 256

/**
  Execute $cmd and (maybe) capture `stdout` and `stderr`

  Commands are executed via `/bin/sh -c`

  $std_out and $std_err are value-result parameters.  Unless
  NULL, they will be changed to point to dynamic memory
  containing the data sent to standard output and
  standard error, respectively.

  Callers can opt to capture standard output, standard error,
  neither or both.  For example:

  <code>
  char *out, *err;

  // capture only standard output
  exec_command("ls /tmp", &out, NULL);

  // get both
  exec_command("cat file", &out, &err);

  // be quiet!
  exec_command("touch file", NULL, NULL);
  </code>

  Currently, because of the specialized needs of Clockwork,
  only the first 255 characters of either output stream will
  be captured.

  On success, returns 0.  On failure, returns non-zero.
 */
int exec_command(const char *cmd, char **std_out, char **std_err)
{
	int proc_stat;
	int out[2], err[2];
	size_t n;
	pid_t pid;
	fd_set fds;
	int nfds = 0;
	int read_stdout = (std_out ? 1 : 0);
	int read_stderr = (std_err ? 1 : 0);
	int nullfd;

	nullfd = open("/dev/null", O_WRONLY);
	if (read_stdout && pipe(out) != 0) { return -1; }
	if (read_stderr && pipe(err) != 0) { return -1; }

	switch (pid = fork()) {

	case -1: /* failed to fork */
		if (read_stdout) {
			close(out[0]);
			close(out[1]);
		}
		if (read_stderr) {
			close(err[0]);
			close(err[1]);
		}
		return -1;

	case 0: /* in child */
		close(0);
		dup2((read_stdout ? out[1] : nullfd), 1);
		dup2((read_stderr ? err[1] : nullfd), 2);

		execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
		exit(42); /* Burma! */

	default: /* in parent */
		DEBUG("exec_command[%u]: Running `%s'", pid, cmd);
		DEBUG("exec_command[%u]: spawned sub-process", pid);

		if (!read_stdout && !read_stderr) { break; }

		FD_ZERO(&fds);

		if (read_stderr) {
			close(err[1]);
			FD_SET(err[0], &fds);
			nfds = (nfds > err[0] ? nfds : err[0]);
			DEBUG("exec_command[%u]: Capturing STDERR; nfds = %u", pid, nfds);
		}

		if (read_stdout) {
			close(out[1]);
			FD_SET(out[0], &fds);
			nfds = (nfds > out[0] ? nfds : out[0]);
			DEBUG("exec_command[%u]: Capturing STDOUT; nfds = %u", pid, nfds);
		}

		nfds++;
		while ((read_stdout || read_stderr) && select(nfds, &fds, NULL, NULL, NULL) > 0) {
			DEBUG("exec_command[%u]: select() returned - something to read", pid);

			if (read_stdout && FD_ISSET(out[0], &fds)) {
				DEBUG("exec_command[%u]: reading STDOUT from child", pid);
				*std_out = xmalloc(EXEC_OUTPUT_MAX * sizeof(char));
				n = read(out[0], *std_out, EXEC_OUTPUT_MAX);
				(*std_out)[n] = '\0';

				FD_CLR(out[0], &fds);
				close(out[0]);
				read_stdout = 0;

				DEBUG(" > read %s' from STDOUT", *std_out);
			}

			if (read_stderr && FD_ISSET(err[0], &fds)) {
				DEBUG("exec_command[%u]: reading STDERR from child", pid);
				*std_err = xmalloc(EXEC_OUTPUT_MAX * sizeof(char));
				n = read(err[0], *std_err, EXEC_OUTPUT_MAX);
				(*std_err)[n] = '\0';

				FD_CLR(err[0], &fds);
				close(err[0]);
				read_stderr = 0;

				DEBUG(" > read %s' from STDERR", *std_err);
			}

			if (read_stdout) { FD_SET(out[0], &fds); }
			if (read_stderr) { FD_SET(err[0], &fds); }

			DEBUG("exec_command[%u]: going to select() again", pid);
		}
	}

	waitpid(pid, &proc_stat, 0);
	if (!WIFEXITED(proc_stat)) {
		DEBUG("exec_command[%u]: terminated abnormally", pid);
		return -1;
	}

	DEBUG("exec_command[%u]: sub-process exited %u", pid, WEXITSTATUS(proc_stat));
	return WEXITSTATUS(proc_stat);
}

