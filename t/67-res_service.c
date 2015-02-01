/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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
#include "../src/resources.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

TESTS {
	subtest {
		struct res_service *service;
		char *key;

		service = res_service_new("service-key");
		key = res_service_key(service);
		is_string(key, "service:service-key", "service key");
		free(key);
		res_service_free(service);
	}

	subtest {
		ok(res_service_norm(NULL, NULL, NULL) == 0, "res_service_norm is a NOOP");
	}

	subtest {
		res_service_free(NULL);
		pass("res_service_free(NULL) doesn't segfault");
	}

	subtest {
#if TEST_AS_ROOT
		struct res_service *r;
		struct resource_env env;
		struct report *report;

		/* so we can clean up after ourselves */
		int was_enabled = 0;
		int was_running = 0;

		env.service_manager = DEFAULT_SM; /* from test/local.h */

		/* first make sure the service is stopped and disabled */
		if (service_enabled(DEFAULT_SM, TEST_SAFE_SVC) == 0) {
			service_disable(DEFAULT_SM, TEST_SAFE_SVC);
			was_enabled = 1;
		}
		if (service_running(DEFAULT_SM, TEST_SAFE_SVC) == 0) {
			service_stop(DEFAULT_SM, TEST_SAFE_SVC);
			was_running = 1;
		}

		ok(service_enabled(DEFAULT_SM, TEST_SAFE_SVC) == 0, "service disabled");
		ok(service_running(DEFAULT_SM, TEST_SAFE_SVC) == 0, "service not running");

		/* now the real test */
		r = res_service_new(TEST_SAFE_SVC);
		res_service_set(r, "enabled", "yes");
		res_service_set(r, "running", "yes");

		ok( ENFORCED(r, RES_SERVICE_RUNNING),  "RUNNING is enforced");
		ok(!ENFORCED(r, RES_SERVICE_STOPPED),  "STOPPED is not enforced");
		ok( ENFORCED(r, RES_SERVICE_ENABLED),  "ENABLED is enforced");
		ok(!ENFORCED(r, RES_SERVICE_DISABLED), "DISABLED is not enforced");

		ok(res_service_stat(r, &env) == 0, "res_service_stat succeeds");
		ok(!r->running, "service not running (non-compliant)");
		ok(!r->enabled, "service not enabled (non-compliant)");

		isnt_null(report = res_service_fixup(r, 0, &env), "res_service fixed up");
		is(report->fixed,     1, "resource was fixed");
		is(report->compliant, 1, "resource was compliant");
		report_free(report);
		sleep(1);

		ok(service_running(DEFAULT_SM, TEST_SAFE_SVC) == 0, "service is running");
		ok(service_enabled(DEFAULT_SM, TEST_SAFE_SVC) == 0, "service is enabled");

		res_service_set(r, "enabled", "no");
		res_service_set(r, "running", "no");

		ok(!ENFORCED(r, RES_SERVICE_RUNNING),  "RUNNING is not enforced");
		ok( ENFORCED(r, RES_SERVICE_STOPPED),  "STOPPED is enforced");
		ok(!ENFORCED(r, RES_SERVICE_ENABLED),  "ENABLED is not enforced");
		ok( ENFORCED(r, RES_SERVICE_DISABLED), "DISABLED is enforced");

		ok(res_service_stat(r, &env) == 0, "res_service_stat succeeds");
		ok(!r->running, "service is running (non-compliant)");
		ok(!r->enabled, "service is enabled (non-compliant)");

		isnt_null(report = res_service_fixup(r, 0, &env), "res_service fixed up");
		is(report->fixed,     1, "resource was fixed");
		is(report->compliant, 1, "resource was compliant");
		report_free(report);
		sleep(1);

		ok(service_running(DEFAULT_SM, TEST_SAFE_SVC) != 0, "service not running");
		ok(service_enabled(DEFAULT_SM, TEST_SAFE_SVC) != 0, "service not enabled");

		/* clean up after ourselves */
		if (was_enabled) {
			service_enable(DEFAULT_SM, TEST_SAFE_SVC);
			sleep(1);
			ok(service_enabled(DEFAULT_SM, TEST_SAFE_SVC) == 0, "service re-enabled");
		} else {
			service_disable(DEFAULT_SM, TEST_SAFE_SVC);
			sleep(1);
			ok(service_enabled(DEFAULT_SM, TEST_SAFE_SVC) != 0, "service re-disabled");
		}

		if (was_running) {
			service_start(DEFAULT_SM, TEST_SAFE_SVC);
			sleep(1);
			ok(service_running(DEFAULT_SM, TEST_SAFE_SVC) == 0, "service re-started");
		} else {
			service_stop(DEFAULT_SM, TEST_SAFE_SVC);
			sleep(1);
			ok(service_running(DEFAULT_SM, TEST_SAFE_SVC) != 0, "service re-stopped");
		}
#endif
	}

	subtest {
		struct res_service *rs;

		rs = res_service_new("svc1");

		res_service_set(rs, "service", "clockd");
		res_service_set(rs, "running", "yes");

		ok(res_service_match(rs, "service", "clockd") == 0, "match service=clockd");
		ok(res_service_match(rs, "service", "apache2") != 0, "!match service=apache2");
		ok(res_service_match(rs, "running", "yes") != 0, "running is not a matchable attr");

		is_int(res_service_set(rs, "what-does-the-fox-say", "ring-ding-ring-ding"),
			-1, "res_service_set doesn't like nonsensical attributes");

		res_service_free(rs);
	}

	subtest {
		struct res_service *r;
		hash_t *h;

		isnt_null(h = vmalloc(sizeof(hash_t)), "created hash");
		isnt_null(r = res_service_new("svc"), "created service");

		ok(res_service_attrs(r, h) == 0, "retrieved attrs");
		is_string(hash_get(h, "name"),    "svc", "h.name");
		is_string(hash_get(h, "running"), "no",  "h.running"); // default
		is_string(hash_get(h, "enabled"), "no",  "h.enabled"); // default

		res_service_set(r, "running", "yes");
		res_service_set(r, "enabled", "yes");

		ok(res_service_attrs(r, h) == 0, "retrieved attrs");
		is_string(hash_get(h, "running"), "yes", "h.running");
		is_string(hash_get(h, "enabled"), "yes", "h.enabled");

		res_service_set(r, "service", "httpd");
		res_service_set(r, "running", "no");
		res_service_set(r, "enabled", "no");

		ok(res_service_attrs(r, h) == 0, "retrieved attrs");
		is_string(hash_get(h, "name"),    "httpd", "h.name");
		is_string(hash_get(h, "running"), "no",    "h.running");
		is_string(hash_get(h, "enabled"), "no",    "h.enabled");

		res_service_set(r, "stopped",  "no");
		res_service_set(r, "disabled", "no");

		ok(res_service_attrs(r, h) == 0, "retrieved attrs");
		is_string(hash_get(h, "running"), "yes", "h.running");
		is_string(hash_get(h, "enabled"), "yes", "h.enabled");

		res_service_set(r, "stopped",  "yes");
		res_service_set(r, "disabled", "yes");

		ok(res_service_attrs(r, h) == 0, "retrieved attrs");
		is_string(hash_get(h, "running"), "no", "h.running");
		is_string(hash_get(h, "enabled"), "no", "h.enabled");

		ok(res_service_set(r, "xyzyy", "BAD") != 0, "xyzzy is a bad attr");
		ok(res_service_attrs(r, h) == 0, "retrieved attrs");
		is_null(hash_get(h, "xyzzy"), "h.xyzzy is unset (bad attr)");

		hash_done(h, 1);
		free(h);
		res_service_free(r);
	}

	done_testing();
}
