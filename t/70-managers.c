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

#include "test.h"
#include "../src/clockwork.h"
#include "../src/service.h"
#include "../src/package.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

TESTS {
	const char *SM = "t/data/managers/svc";
	const char *PM = "t/data/managers/pkg";

	subtest {
		/* running-service */
		is_int(service_running(SM, "running-service"), 0,
				"running-service is service_running()");
		is_int(service_enabled(SM, "running-service"), 0,
				"running-service is service_enabled()");

		is_int(service_start(SM, "running-service"), 1,
				"running-service cannot be started");

		is_int(service_stop(SM, "running-service"), 0,
				"running-service can be stopped");
		is_int(service_restart(SM, "running-service"), 0,
				"running-service can be restarted");
		is_int(service_reload(SM, "running-service"), 0,
				"running-service can be reloaded");

		is_int(service_enable(SM, "running-service"), 0,
				"running-service can be enabled");
		is_int(service_disable(SM, "running-service"), 0,
				"running-service can be disabled");


		/* stopped-service */
		is_int(service_running(SM, "stopped-service"), 1,
				"stopped-service is not service_running()");
		is_int(service_enabled(SM, "stopped-service"), 1,
				"stopped-service is not service_enabled()");

		is_int(service_start(SM, "stopped-service"), 0,
				"stopped-service can be started");
		is_int(service_stop(SM, "stopped-service"), 1,
				"stopped-service cannot be stopped");
		is_int(service_restart(SM, "stopped-service"), 0,
				"stopped-service can be restarted");
		is_int(service_reload(SM, "stopped-service"), 1,
				"stopped-service cannot be reloaded");

		is_int(service_enable(SM, "stopped-service"), 0,
				"stopped-service can be enabled");
		is_int(service_disable(SM, "stopped-service"), 0,
				"stopped-service can be disabled");


		/* bogus */
		is_int(service_running(SM, "bogus"), 1,
				"bogus is not service_running()");
		is_int(service_enabled(SM, "bogus"), 1,
				"bogus is not service_enabled()");

		is_int(service_start(SM, "bogus"), 1,
				"bogus cannot be started");
		is_int(service_stop(SM, "bogus"), 1,
				"bogus cannot be stopped");
		is_int(service_restart(SM, "bogus"), 1,
				"bogus cannot be restarted");
		is_int(service_reload(SM, "bogus"), 1,
				"bogus cannot be reloaded");

		is_int(service_enable(SM, "bogus"), 1,
				"bogus cannot be enabled");
		is_int(service_disable(SM, "bogus"), 1,
				"bogus cannot be disabled");
	}

	subtest {
		char *version;

		/* simple - installed package */
		version = package_version(PM, "simple");
		is_string(version, "1.2.3-4", "Found version of 'simple'");
		free(version);

		is_int(package_query(PM, "simple", NULL), 0,
				"package 'simple' is installed");
		is_int(package_query(PM, "simple", "1.2.3-4"), 0,
				"package simple-1.2.3-4 is installed");
		is_int(package_query(PM, "simple", "1.2.8-7"), 2,
				"package simple needs upgrade to v1.2.8-7");

		version = package_latest(PM, "simple");
		is_string(version, "1.3.7-1", "Latest version of 'simple'");
		free(version);

		is_int(package_install(PM, "simple", NULL), 0,
				"package 'simple' (any version) is installable");
		is_int(package_install(PM, "simple", "2.3.4"), 0,
				"package simple-2.3.4 is installable");

		is_int(package_remove(PM, "simple"), 0,
				"package 'simple' is removable");


		/* multiline - installed package */
		version = package_version(PM, "multiline");
		is_string(version, "1.2.3-4", "Found version of 'multiline'");
		free(version);

		is_int(package_query(PM, "multiline", NULL), 0,
				"package 'multiline' is installed");
		is_int(package_query(PM, "multiline", "1.2.3-4"), 0,
				"package multiline-1.2.3-4 is installed");
		is_int(package_query(PM, "multiline", "1.2.8-7"), 2,
				"package multiline needs upgrade to v1.2.8-7");

		version = package_latest(PM, "multiline");
		is_string(version, "1.3.7-1", "Latest version of 'multiline'");
		free(version);

		is_int(package_install(PM, "multiline", NULL), 0,
				"package 'multiline' (any version) is installable");
		is_int(package_install(PM, "multiline", "2.3.4"), 0,
				"package multiline-2.3.4 is installable");

		is_int(package_remove(PM, "multiline"), 0,
				"package 'multiline' is removable");


		/* absent - installed package */
		version = package_version(PM, "absent");
		ok(!version, "No version found for 'absent'");

		is_int(package_query(PM, "absent", NULL), 1,
				"package 'absent' is not installed");
		is_int(package_query(PM, "absent", "1.2.3-4"), 1,
				"package absent-1.2.3-4 is not installed");
		is_int(package_query(PM, "absent", "1.2.8-7"), 1,
				"package absent needs is not installed");

		version = package_latest(PM, "absent");
		ok(!version, "no latest version for package 'absent'");

		is_int(package_install(PM, "absent", NULL), 1,
				"package 'absent' (any version) is not installable");
		is_int(package_install(PM, "absent", "2.3.4"), 1,
				"package absent-2.3.4 is not installable");

		is_int(package_remove(PM, "absent"), 1,
				"package 'absent' is not removable");
	}

	done_testing();
}
