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

#include "test.h"
#include "../src/clockwork.h"
#include "../src/resources.h"

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
		ok(res_sysctl_norm(NULL, NULL, NULL) == 0, "res_sysctl_norm is a NOOP");
	}

	subtest {
		res_sysctl_free(NULL);
		pass("res_sysctl_free(NULL) doesn't segfault");
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
		cw_hash_t *h;

		isnt_null(h = cw_alloc(sizeof(cw_hash_t)), "created hash");
		isnt_null(rs = res_sysctl_new("net.ipv4.tun"), "created sysctl");

		ok(res_sysctl_attrs(rs, h) == 0, "got res_sysctl attrs");
		is_string(cw_hash_get(h, "param"),   "net.ipv4.tun", "h.param");
		is_string(cw_hash_get(h, "persist"), "no",           "h.persist");
		is_null(cw_hash_get(h, "value"), "h.value is unset");

		res_sysctl_set(rs, "value", "42");
		res_sysctl_set(rs, "persist", "yes");

		ok(res_sysctl_attrs(rs, h) == 0, "got res_sysctl attrs");
		is_string(cw_hash_get(h, "param"),   "net.ipv4.tun", "h.param");
		is_string(cw_hash_get(h, "value"),   "42",           "h.value");
		is_string(cw_hash_get(h, "persist"), "yes",          "h.persist");

		res_sysctl_set(rs, "persist", "no");

		ok(res_sysctl_attrs(rs, h) == 0, "got res_sysctl attrs");
		is_string(cw_hash_get(h, "persist"), "no",           "h.persist");

		ok(res_sysctl_set(rs, "xyzzy", "BAD") != 0, "xyzzy is not a valid attribute");
		ok(res_sysctl_attrs(rs, h) == 0, "got res_sysctl attrs");
		is_null(cw_hash_get(h, "xyzzy"), "h.xyzzy is NULL (bad attr)");

		cw_hash_done(h, 1);
		free(h);
		res_sysctl_free(rs);
	}

	done_testing();
}
