#ifndef _DAEMON_H
#define _DAEMON_H

#include <sys/types.h>

struct daemon {
	const char *name;
	const char *lock_file;
	const char *pid_file;
};

void daemonize(const struct daemon *d);

void daemon_acquire_lock(const char *path);
void daemon_fork1(void);
void daemon_fork2(const char *path);
void daemon_write_pid(pid_t pid, const char *path);
void daemon_settle(void);

#endif
