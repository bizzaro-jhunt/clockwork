/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

#include "../src/base.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv)
{
	proc_t ps;
	pid_t pid = (argc == 2 ? atoi(argv[1]) : getpid());
	if (proc_stat(pid, &ps) != 0) {
		printf("proc_stat(%u) failed: %m", pid);
		exit(1);
	}

	printf("pid %u; ppid %u; state %c; uid %u/%u; gid %u/%u\n",
		ps.pid, ps.ppid, ps.state, ps.uid, ps.euid, ps.gid, ps.egid);
	exit(0);
}
