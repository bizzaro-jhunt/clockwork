#include "pkgmgr.h"

#include <sys/wait.h>

const struct package_manager PM_dpkg_apt = {
	.query_local     = "dpkg-query -W -f='${Version}' %s",
	.install_latest  = "apt-get install -qqy %s",
	.install_version = "apt-get install -qqy %s=%s-*",
	.remove          = "apt-get purge -qqy %s"
};

const struct package_manager PM_rpm_yum = {
	.query_local     = "rpm --quiet --qf='%%{VERSION}-%%{RELEASE}' -q %s",
	.install_latest  = "yum install -qy %s",
	.install_version = "yum install -qy %s-%s",
	.remove          = "yum erase -qy %s"
};

static int sub_command(const char *cmd, char **buf)
{
	int proc_stat;
	int fd[2];
	size_t n;
	pid_t pid;

	if (buf) {
		pipe(fd);
	}

	switch (pid = fork()) {
	case -1:
		if (buf) {
			close(fd[0]);
			close(fd[1]);
		}
		return -1;
	case 0:
		close(1); close(2);
		if (buf) { dup(fd[1]); }
		close(0);
		execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
		exit(42); /* Burma! */
	default:
		DEBUG("sub_command[%u]: Running `%s'", pid, cmd);
		DEBUG("sub_command[%u]: spawned sub-process", pid);

		if (buf) {
			close(fd[1]);

			/* FIXME: only handles "small" output... */
			*buf = xmalloc(256 * sizeof(char));
			n = read(fd[0], *buf, 256);
			(*buf)[n] = '\0';

			close(fd[0]);
		}
	}

	waitpid(pid, &proc_stat, 0);
	if (!WIFEXITED(proc_stat)) {
		DEBUG("sub_command[%u]: terminated abnormally", pid);
		return -1;
	}

	DEBUG("sub_command[%u]: sub-process exited %u", pid, WEXITSTATUS(proc_stat));
	return WEXITSTATUS(proc_stat);
}

char* package_manager_query(const struct package_manager *mgr, const char *pkg)
{
	char *cmd, *version, *v;
	int code;

	if (!mgr || !pkg) { return NULL ;}

	cmd = string(mgr->query_local, pkg);
	code = sub_command(cmd, &version);
	free(cmd);

	for (v = version; *v && *v != '-'; v++ )
		;
	if (*v == '-') { *v = '\0'; }

	return (*version == '\0' ? NULL : version);
}

int package_manager_install(const struct package_manager *mgr, const char *pkg, const char *version)
{
	char *cmd;
	int code;

	if (!mgr || !pkg) { return -1; }

	cmd = (version ? string(mgr->install_version, pkg, version)
	               : string(mgr->install_latest,  pkg));
	code = sub_command(cmd, NULL);
	free(cmd);
	return code;
}

int package_manager_remove(const struct package_manager *mgr, const char *pkg)
{
	char *cmd;
	int code;

	if (!mgr || !pkg) { return -1; }

	cmd = string(mgr->remove, pkg);
	code = sub_command(cmd, NULL);
	free(cmd);
	return code;
}

