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
#include "../src/resources.h"
#include "../src/augcw.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

TESTS {
	subtest {
		struct res_sysctl   *sysctl;
		char *key;

		sysctl = res_sysctl_new("kernel.param1");
		key = res_sysctl_key(sysctl);
		is_string(key, "sysctl:kernel.param1", "res_sysctl key");
		free(key);
		res_sysctl_free(sysctl);
	}

	subtest {
		ok(res_sysctl_notify(NULL, NULL)     == 0, "res_sysctl_notify is a NOOP");
		ok(res_sysctl_norm(NULL, NULL, NULL) == 0, "res_sysctl_norm is a NOOP");
	}

	subtest {
		res_sysctl_free(NULL);
		pass("res_sysctl_free(NULL) doesn't segfault");
	}

	subtest {
		struct res_sysctl *r;
		struct resource_env env;

		r = res_sysctl_new("martians");
		res_sysctl_set(r, "param", "net.ipv4.conf.all.log_martians");
		res_sysctl_set(r, "value", "1");
		res_sysctl_set(r, "persist", "yes");

		/* set up resource env with augeas stuff */
		isnt_null(env.aug_context = augcw_init(), "Augeas initialized");

		/* test setup ensures that log_martians is 0 in /proc/sys */
		ok(res_sysctl_stat(r, &env) == 0, "res_sysctl_stat succeeds");
#if TEST_AS_ROOT
		ok(DIFFERENT(r, RES_SYSCTL_VALUE), "VALUE is out of compliance");
#endif

#if TEST_AS_ROOT
		struct report *report;
		isnt_null(report = res_sysctl_fixup(r, 0, &env), "res_sysctl fixed up");
		is_int(report->fixed,     1, "sysctl is fixed");
		is_int(report->compliant, 1, "sysctl is compliant");

		ok(res_sysctl_stat(r, &env) == 0, "res_sysctl_stat succeeds");
		ok(!DIFFERENT(r, RES_SYSCTL_VALUE), "VALUE is now in compliance");
		report_free(report);
#endif

		is_int(res_sysctl_set(r, "what-does-the-fox-say", "ring-ding-ring-ding"),
			-1, "res_sysctl_set doesn't like nonsensical attributes");

		res_sysctl_free(r);
	}

	subtest {
		struct res_sysctl *rs;

		rs = res_sysctl_new("sys.ctl");
		res_sysctl_set(rs, "value", "0 5 6 1");
		res_sysctl_set(rs, "persist", "yes");

		ok(res_sysctl_match(rs, "param", "sys.ctl") == 0, "match param=sys.ctl");
		ok(res_sysctl_match(rs, "param", "sys.p")   != 0, "!match param=sys.p");

		ok(res_sysctl_match(rs, "value", "0 5 6 1") != 0, "value is not a matchable attr");
		ok(res_sysctl_match(rs, "persist", "yes")   != 0, "persist is not a matchable attr");

		res_sysctl_free(rs);
	}

	subtest {
		struct res_sysctl *rs;
		char *packed;
		const char *expected;

		rs = res_sysctl_new("ip");
		res_sysctl_set(rs, "param", "kernel.net.ip");
		res_sysctl_set(rs, "value", "1");
		res_sysctl_set(rs, "persist", "yes");

		packed = res_sysctl_pack(rs);
		expected = "res_sysctl::\"ip\"00000003\"kernel.net.ip\"\"1\"0001";
		is_string(packed, expected, "sysctl packs properly");
		free(packed);

		res_sysctl_set(rs, "persist", "no");
		packed = res_sysctl_pack(rs);
		expected = "res_sysctl::\"ip\"00000001\"kernel.net.ip\"\"1\"0000";
		is_string(packed, expected, "sysctl packs properly");
		free(packed);

		res_sysctl_free(rs);
	}

	subtest {
		struct res_sysctl *rs;
		const char *packed;

		packed = "res_sysctl::\"param\""
			"00000003"
			"\"net.ipv4.echo\""
			"\"2\""
			"0001";

		is_null(res_sysctl_unpack("<invalid pack data>"), "res_sysctl_unpack handles bad data");
		isnt_null(rs = res_sysctl_unpack(packed), "res_sysctl_unpacks properly");

		is_string(rs->key,     "param",         "unpacked sysctl key");
		is_string(rs->param,   "net.ipv4.echo", "unpacked sysctl param");
		is_string(rs->value,   "2",             "unpacked sysctl value");
		is_int(   rs->persist, 1,               "unpacked sysctl persist");

		ok(ENFORCED(rs, RES_SYSCTL_VALUE),   "VALUE is enforced");
		ok(ENFORCED(rs, RES_SYSCTL_PERSIST), "PERSIST is enforced");

		res_sysctl_free(rs);
	}

	subtest {
		struct res_sysctl *rs;
		struct hash *h;

		isnt_null(h = hash_new(), "created hash");
		isnt_null(rs = res_sysctl_new("net.ipv4.tun"), "created sysctl");

		ok(res_sysctl_attrs(rs, h) == 0, "got res_sysctl attrs");
		is_string(hash_get(h, "param"),   "net.ipv4.tun", "h.param");
		is_string(hash_get(h, "persist"), "no",           "h.persist");
		is_null(hash_get(h, "value"), "h.value is unset");

		res_sysctl_set(rs, "value", "42");
		res_sysctl_set(rs, "persist", "yes");

		ok(res_sysctl_attrs(rs, h) == 0, "got res_sysctl attrs");
		is_string(hash_get(h, "param"),   "net.ipv4.tun", "h.param");
		is_string(hash_get(h, "value"),   "42",           "h.value");
		is_string(hash_get(h, "persist"), "yes",          "h.persist");

		res_sysctl_set(rs, "persist", "no");

		ok(res_sysctl_attrs(rs, h) == 0, "got res_sysctl attrs");
		is_string(hash_get(h, "persist"), "no",           "h.persist");

		ok(res_sysctl_set(rs, "xyzzy", "BAD") != 0, "xyzzy is not a valid attribute");
		ok(res_sysctl_attrs(rs, h) == 0, "got res_sysctl attrs");
		is_null(hash_get(h, "xyzzy"), "h.xyzzy is NULL (bad attr)");

		hash_free_all(h);
		res_sysctl_free(rs);
	}

	done_testing();
}
