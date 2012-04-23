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

#include "package.h"
#include "../exec.h"

#define DEFINE_PM_QUERY(t)    int  package_manager_ ## t ## _query(const char *package, const char *version)
#define DEFINE_PM_VERSION(t) char* package_manager_ ## t ## _version(const char *package)
#define DEFINE_PM_LATEST(t)  char* package_manager_ ## t ## _latest(const char *package)
#define DEFINE_PM_INSTALL(t)  int  package_manager_ ## t ## _install(const char *package, const char *version)
#define DEFINE_PM_REMOVE(t)   int  package_manager_ ## t ## _remove(const char *package)

#define NEW_PACKAGE_MANAGER(t) \
DEFINE_PM_QUERY(t); \
DEFINE_PM_VERSION(t); \
DEFINE_PM_LATEST(t); \
DEFINE_PM_INSTALL(t); \
DEFINE_PM_REMOVE(t); \
const struct package_manager PM_ ## t ## _struct = { \
	.query   = package_manager_ ## t ## _query, \
	.version = package_manager_ ## t ## _version, \
	.latest  = package_manager_ ## t ## _latest, \
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

	command = string("/usr/bin/dpkg-query -W -f='${Version}' %s", package);
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

DEFINE_PM_LATEST(dpkg_apt) {
	/* FIXME: figure out how to get latest available pkg version on debian */
	return NULL;
}

DEFINE_PM_INSTALL(dpkg_apt) {
	char *command;
	int rc;

	command = (version ? string("/usr/bin/apt-get install -qqy %s=%s-*", package, version)
	                   : string("/usr/bin/apt-get install -qqy %s", package));
	rc = exec_command(command, NULL, NULL);
	free(command);

	return rc;
}

DEFINE_PM_REMOVE(dpkg_apt) {
	char *command;
	int rc;

	command = string("/usr/bin/apt-get purge -qqy %s", package);
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

	command = string("/bin/rpm --qf='%%{VERSION}' -q %s | /bin/grep -v 'is not installed$'", package);
	rc = exec_command(command, &version, NULL);
	free(command);

	if (*version == '\0') {
		free(version);
		return NULL;
	}

	return version;
}

DEFINE_PM_LATEST(rpm_yum) {
	char *command;
	char *version;
	int rc;

	command = string("/usr/bin/repoquery --qf '%%{VERSION}' -q %s", package);
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

	command = (version ? string("/usr/bin/yum install -qy %s-%s", package, version)
	                   : string("/usr/bin/yum install -qy %s", package));
	rc = exec_command(command, NULL, NULL);
	free(command);

	return rc;
}

DEFINE_PM_REMOVE(rpm_yum) {
	char *command;
	int rc;

	command = string("/usr/bin/yum erase -qy %s", package);
	rc = exec_command(command, NULL, NULL);
	free(command);

	return rc;
}
