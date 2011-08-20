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

#undef DEFINE_SM_ACTION
#undef DEFINE_SM_STATUS
#undef NEW_SERVICE_MANAGER

