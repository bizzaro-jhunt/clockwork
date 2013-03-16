/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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

extern const struct service_manager *SM_debian;
extern const struct service_manager *SM_chkconfig;

#define service_enabled(sm,service) ((*((sm)->status))((service), 0))
#define service_running(sm,service) ((*((sm)->status))((service), 1))

#define service_start(sm,service)   ((*((sm)->action))((service), SERVICE_MANAGER_START))
#define service_stop(sm,service)    ((*((sm)->action))((service), SERVICE_MANAGER_STOP))
#define service_restart(sm,service) ((*((sm)->action))((service), SERVICE_MANAGER_RESTART))
#define service_reload(sm,service)  ((*((sm)->action))((service), SERVICE_MANAGER_RELOAD))
#define service_enable(sm,service)  ((*((sm)->action))((service), SERVICE_MANAGER_ENABLE))
#define service_disable(sm,service) ((*((sm)->action))((service), SERVICE_MANAGER_DISABLE))

#endif
