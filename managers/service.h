#ifndef _MANAGERS_SERVICE_H
#define _MANAGERS_SERVICE_H

#include "../clockwork.h"

enum service_manager_action {
	SERVICE_MANAGER_START = 1,
	SERVICE_MANAGER_STOP,
	SERVICE_MANAGER_RESTART,
	SERVICE_MANAGER_RELOAD,
	SERVICE_MANAGER_ENABLE,
	SERVICE_MANAGER_DISABLE
};

typedef int (*service_manager_status_f)(const char *service, unsigned int run);
typedef int (*service_manager_action_f)(const char *service, enum service_manager_action action);

struct service_manager {
	service_manager_status_f status;
	service_manager_action_f action;
};

const struct service_manager *SM_debian;

#define service_enabled(sm,service) ((*((sm)->status))((service), 0))
#define service_running(sm,service) ((*((sm)->status))((service), 1))

#define service_start(sm,service)   ((*((sm)->action))((service), SERVICE_MANAGER_START))
#define service_stop(sm,service)    ((*((sm)->action))((service), SERVICE_MANAGER_STOP))
#define service_restart(sm,service) ((*((sm)->action))((service), SERVICE_MANAGER_RESTART))
#define service_reload(sm,service)  ((*((sm)->action))((service), SERVICE_MANAGER_RELOAD))
#define service_enable(sm,service)  ((*((sm)->action))((service), SERVICE_MANAGER_ENABLE))
#define service_disable(sm,service) ((*((sm)->action))((service), SERVICE_MANAGER_DISABLE))

#endif
