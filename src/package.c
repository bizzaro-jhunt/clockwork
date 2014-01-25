/*
  Copyright 2011-2014 James Hunt <james@niftylogic.com>

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
#include "exec.h"

static int
_package_exec(const char *mgr, const char *action, const char *pkg, const char *v, char **out)
{
	char *command;
	int rc;

	command = string("%s %s %s%s%s",
			mgr, action, pkg, (v ? " " : ""), (v ? v : ""));
	rc = exec_command(command, out, NULL);
	free(command);
	return rc;
}

static void
_head1(char *buf)
{
	char *p;
	if (p = strchr(buf, '\n')) { *p = '\0'; }
	if (p = strchr(buf, '\r')) { *p = '\0'; }
}

int
package_query(const char *mgr, const char *pkg, const char *ver)
{
	char *v;
	v = package_version(mgr, pkg);
	if (!v) {
		return 1;
	}

	if (ver && strcmp(v, ver) != 0) {
		free(v);
		/* installed, but wrong version */
		return 2;
	}

	free(v);
	return 0;
}

char *
package_version(const char *mgr, const char *pkg)
{
	char *version;
	int rc;

	rc = _package_exec(mgr, "version", pkg, NULL, &version);

	if (rc != 0 || *version == '\0') {
		free(version);
		return NULL;
	}

	_head1(version);
	return version;
}

char *
package_latest(const char *mgr, const char *pkg)
{
	char *version, *p;
	int rc;

	rc = _package_exec(mgr, "latest", pkg, NULL, &version);

	if (*version == '\0' || rc != 0) {
		free(version);
		return NULL;
	}

	_head1(version);
	return version;
}

int
package_install(const char *mgr, const char *pkg, const char *ver)
{
	return _package_exec(mgr, "install", pkg, ver, NULL);
}

int
package_remove(const char *mgr, const char *pkg)
{
	return _package_exec(mgr, "remove", pkg, NULL, NULL);
}
