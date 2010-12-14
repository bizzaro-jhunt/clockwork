#include "daemon.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

static void daemon_acquire_lock(const char *path);
static void daemon_fork1(void);
static void daemon_fork2(const char *path);
static void daemon_write_pid(pid_t pid, const char *path);
static void daemon_settle(void);

void daemonize(const struct daemon *d)
{
	/* are we already a child of init? */
	if (getppid() == 1) { return; }

	daemon_fork1();
	daemon_fork2(d->pid_file);
	daemon_acquire_lock(d->lock_file);
	daemon_settle();
}

static void daemon_acquire_lock(const char *path)
{

	int lock_fd;

	if (!(path && path[0])) {
		ERROR("NULL or empty lock file path given");
		exit(2);
	}

	lock_fd = open(path, O_CREAT | O_RDWR, 0640);
	if (lock_fd < 0) {
		ERROR("unable to open lock file %s: %s", path, strerror(errno));
		exit(2);
	}

	if (lockf(lock_fd, F_TLOCK, 0) < 0) {
		ERROR("unable to lock %s; daemon already running?", path);
		exit(2);
	}

}

static void daemon_fork1(void)
{
	pid_t pid = fork();

	if (pid < 0) {
		ERROR("unable to fork: %s", strerror(errno));
		exit(2);
	}

	if (pid > 0) { /* parent process */
		_exit(0);
	}
}

static void daemon_fork2(const char *path)
{
	pid_t pid, sessid;

	sessid = setsid(); /* new process session / group */
	if (sessid < 0) {
		ERROR("unable to create new process group: %s", strerror(errno));
		exit(2);
	}

	pid = fork();
	if (pid < 0) {
		ERROR("unable to fork again: %s", strerror(errno));
		exit(2);
	}

	if (pid > 0) { /* "middle" child / parent */
		/* Now is the only time we have access to the PID of
		   the fully daemonized process. */
		daemon_write_pid(pid, path);
		_exit(0);
	}
}

static void daemon_write_pid(pid_t pid, const char *path)
{
	FILE *io;

	io = fopen(path, "w");
	if (!io) {
		ERROR("unable to open PID file %s for writing: %s", path, strerror(errno));
		exit(2);
	}
	fprintf(io, "%lu\n", (unsigned long)pid);
	fclose(io);
}

static void daemon_settle(void)
{
	umask(0); /* reset the file umask */
	if (chdir("/") < 0) {
		ERROR("unable to chdir to /: %s", strerror(errno));
		exit(2);
	}

	/* redirect standard fds */
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
}

