#include "../src/cw.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv)
{
	cw_proc_t ps;
	pid_t pid = (argc == 2 ? atoi(argv[1]) : getpid());
	if (cw_proc_stat(pid, &ps) != 0) {
		printf("cw_proc_stat(%u) failed: %m", pid);
		exit(1);
	}

	printf("pid %u; ppid %u; state %c; uid %u/%u; gid %u/%u\n",
		ps.pid, ps.ppid, ps.state, ps.uid, ps.euid, ps.gid, ps.egid);
	exit(0);
}
