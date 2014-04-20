/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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
#include "exec.h"

static int
_service_exec(const char *mgr, const char *action, const char *svc)
{
	char *command;
	int rc;

	command = string("%s %s %s",
			mgr, action, svc);
	rc = exec_command(command, NULL, NULL);
	free(command);
	return rc;
}

int
service_enabled(const char *mgr, const char *svc)
{
	return _service_exec(mgr, "boot-status", svc);
}

int
service_running(const char *mgr, const char *svc)
{
	return _service_exec(mgr, "run-status", svc);
}

int
service_start(const char *mgr, const char *svc)
{
	return _service_exec(mgr, "start", svc);
}

int
service_stop(const char *mgr, const char *svc)
{
	return _service_exec(mgr, "stop", svc);
}

int
service_restart(const char *mgr, const char *svc)
{
	return _service_exec(mgr, "restart", svc);
}

int
service_reload(const char *mgr, const char *svc)
{
	return _service_exec(mgr, "reload", svc);
}

int
service_enable(const char *mgr, const char *svc)
{
	return _service_exec(mgr, "enable", svc);
}

int
service_disable(const char *mgr, const char *svc)
{
	return _service_exec(mgr, "disable", svc);
}
