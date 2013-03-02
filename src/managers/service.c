/*
  Copyright 2011-2013 James Hunt <james@jameshunt.us>

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

#include "service.h"
#include "../exec.h"

/******************************************************************************/

#define DEFINE_SM_STATUS(t) int service_manager_ ## t ## _status(const char *service, unsigned int run)
#define DEFINE_SM_ACTION(t) int service_manager_ ## t ## _action(const char *service, enum service_manager_action action)

#define NEW_SERVICE_MANAGER(t) \
DEFINE_SM_STATUS(t); \
DEFINE_SM_ACTION(t); \
const struct service_manager SM_ ## t ## _struct = { \
	.status = service_manager_ ## t ## _status, \
	.action = service_manager_ ## t ## _action  }; \
const struct service_manager *SM_ ## t = &SM_ ## t ## _struct

NEW_SERVICE_MANAGER(debian);

DEFINE_SM_STATUS(debian) {
	char *command;
	int rc;

	if (run) {
		command = string("/etc/init.d/%s status", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);

		return ((rc == 7 || rc == 1) ? 1 : (rc == 0 ? 0 : -1));
	} else {
		command = string("/usr/sbin/invoke-rc.d --quiet --query %s start", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);

		return rc == 104 ? 0 : 1;
	}
}

DEFINE_SM_ACTION(debian) {
	char *command;
	int rc;

	switch (action) {
	case SERVICE_MANAGER_START:
		command = string("/etc/init.d/%s start", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_STOP:
		command = string("/etc/init.d/%s stop", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_RESTART:
		command = string("/etc/init.d/%s restart", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_RELOAD:
		command = string("/etc/init.d/%s reload", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_ENABLE:
		command = string("/usr/sbin/update-rc.d -f %s remove", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		command = string("/usr/sbin/update-rc.d %s defaults", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_DISABLE:
		command = string("/usr/sbin/update-rc.d -f %s remove", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	default:
		/* invalid command */
		return -1;
	}
}

NEW_SERVICE_MANAGER(chkconfig);

DEFINE_SM_STATUS(chkconfig) {
	char *command;
	int rc;

	if (run) {
		command = string("/etc/init.d/%s status", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);

		return (rc == 1 ? 1 : (rc == 0 ? 0 : -1));
	} else {
		command = string("/sbin/chkconfig --list %s | grep ':on'", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);

		return (rc == 0 ? 0 : 1);
	}
}

DEFINE_SM_ACTION(chkconfig)
{
	char *command;
	int rc;

	switch (action) {
	case SERVICE_MANAGER_START:
		command = string("/etc/init.d/%s start", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_STOP:
		command = string("/etc/init.d/%s stop", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_RESTART:
		command = string("/etc/init.d/%s restart", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_RELOAD:
		command = string("/etc/init.d/%s reload", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_ENABLE:
		command = string("/sbin/chkconfig %s on", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	case SERVICE_MANAGER_DISABLE:
		command = string("/sbin/chkconfig %s off", service);
		rc = exec_command(command, NULL, NULL);
		DEBUG("%s: `%s' returned %u", __func__, command, rc);
		free(command);
		return rc;

	default:
		/* invalid command */
		return -1;
	}
}

#undef DEFINE_SM_ACTION
#undef DEFINE_SM_STATUS
#undef NEW_SERVICE_MANAGER

