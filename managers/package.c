#include "package.h"
#include "../exec.h"

#define DEFINE_PM_QUERY(t)    int  package_manager_ ## t ## _query(const char *package, const char *version)
#define DEFINE_PM_VERSION(t) char* package_manager_ ## t ## _version(const char *package)
#define DEFINE_PM_INSTALL(t)  int  package_manager_ ## t ## _install(const char *package, const char *version)
#define DEFINE_PM_REMOVE(t)   int  package_manager_ ## t ## _remove(const char *package)

#define NEW_PACKAGE_MANAGER(t) \
DEFINE_PM_QUERY(t); \
DEFINE_PM_VERSION(t); \
DEFINE_PM_INSTALL(t); \
DEFINE_PM_REMOVE(t); \
const struct package_manager PM_ ## t ## _struct = { \
	.query   = package_manager_ ## t ## _query, \
	.version = package_manager_ ## t ## _version, \
	.install = package_manager_ ## t ## _install, \
	.remove  = package_manager_ ## t ## _remove }; \
const struct package_manager *PM_ ## t = &PM_ ## t ## _struct

NEW_PACKAGE_MANAGER(dpkg_apt);

DEFINE_PM_QUERY(dpkg_apt) {
	char *v;

	v = package_version(PM_dpkg_apt, package);
	if (!v) {
		return 1;
	}

	if (version && strcmp(v, version) != 0) {
		free(v);
		/* installed, but wrong version */
		return 2;
	}

	free(v);
	return 0;
}

DEFINE_PM_VERSION(dpkg_apt) {
	char *command;
	char *version, *v;
	int rc;

	command = string("dpkg-query -W -f='${Version}' %s", package);
	rc = exec_command(command, &version, NULL);
	free(command);

	if (*version == '\0') {
		free(version);
		return NULL;
	}

	for (v = version; *v && *v != '-'; v++ )
		;
	if (*v == '-') { *v = '\0'; }

	return version;
}

DEFINE_PM_INSTALL(dpkg_apt) {
	char *command;
	int rc;

	command = (version ? string("apt-get install -qqy %s=%s-*", package, version)
	                   : string("apt-get install -qqy %s", package));
	rc = exec_command(command, NULL, NULL);
	free(command);

	return rc;
}

DEFINE_PM_REMOVE(dpkg_apt) {
	char *command;
	int rc;

	command = string("apt-get purge -qqy %s", package);
	rc = exec_command(command, NULL, NULL);
	free(command);

	return rc;
}


NEW_PACKAGE_MANAGER(rpm_yum);

DEFINE_PM_QUERY(rpm_yum) {
	char *v;

	v = package_version(PM_rpm_yum, package);
	if (!v) {
		return 1;
	}

	if (version && strcmp(v, version) != 0) {
		free(v);
		/* installed, but wrong version */
		return 2;
	}

	free(v);
	return 0;
}

DEFINE_PM_VERSION(rpm_yum) {
	char *command;
	char *version;
	int rc;

	command = string("rpm --quiet --qf='%%{VERSION}' -q %s", package);
	rc = exec_command(command, &version, NULL);
	free(command);

	if (*version == '\0') {
		free(version);
		return NULL;
	}

	return version;
}

DEFINE_PM_INSTALL(rpm_yum) {
	char *command;
	int rc;

	command = (version ? string("yum install -qy %s", package, version)
	                   : string("yum install -qy %s-%s", package));
	rc = exec_command(command, NULL, NULL);
	free(command);

	return rc;
}

DEFINE_PM_REMOVE(rpm_yum) {
	char *command;
	int rc;

	command = string("yum erase -qy %s", package);
	rc = exec_command(command, NULL, NULL);
	free(command);

	return rc;
}
