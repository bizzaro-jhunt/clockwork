#include "pkgmgr.h"
#include "exec.h"

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

char* package_manager_query(const struct package_manager *mgr, const char *pkg)
{
	char *cmd, *version, *v;
	int code;

	if (!mgr || !pkg) { return NULL ;}

	cmd = string(mgr->query_local, pkg);
	code = exec_command(cmd, &version, NULL);
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
	code = exec_command(cmd, NULL, NULL);
	free(cmd);
	return code;
}

int package_manager_remove(const struct package_manager *mgr, const char *pkg)
{
	char *cmd;
	int code;

	if (!mgr || !pkg) { return -1; }

	cmd = string(mgr->remove, pkg);
	code = exec_command(cmd, NULL, NULL);
	free(cmd);
	return code;
}

