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
#include "../src/package.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

TESTS {
	subtest {
		struct res_package *package;
		char *key;

		package = res_package_new("package-key");
		key = res_package_key(package);
		is_string(key, "package:package-key", "package key");
		free(key);
		res_package_free(package);
	}

	subtest {
		ok(res_package_notify(NULL, NULL)     == 0, "res_package_notify is a NOOP");
		ok(res_package_norm(NULL, NULL, NULL) == 0, "res_package_norm is a NOOP");
	}

	subtest {
		res_package_free(NULL);
		pass("res_package_free(NULL) doesn't segfault");
	}

	subtest {
		struct res_package *rp;
		rp = res_package_new("sys-tools");

		res_package_set(rp, "name", "lsof");
		res_package_set(rp, "installed", "yes");

		ok(res_package_match(rp, "name", "lsof")   == 0, "match name=lsof");
		ok(res_package_match(rp, "name", "dtrace") != 0, "!match name=dtrace");

		ok(res_package_match(rp, "installed", "yes") != 0, "installed is not a matchable attr");

		is_int(res_package_set(rp, "what-does-the-fox-say", "ring-ding-ring-ding"),
			-1, "res_package_set doesn't like nonsensical attributes");

		res_package_free(rp);
	}

	subtest {
		struct res_package *r;
		char *expected = "res_package::\"pkg-name\""
		                 "00000000"
		                 "\"pkg-name\""
		                 "\"1.2.3-5\"";
		char *actual;

		isnt_null(r = res_package_new("pkg-name"), "generated package");
		res_package_set(r, "version", "1.2.3-5");

		isnt_null(actual = res_package_pack(r), "pack succeeded");
		is_string(actual, expected, "packed properly");

		res_package_free(r);
		free(actual);
	}

	subtest {
		struct res_package *r;
		char *packed;

		packed = "res_package::\"pkgkey\""
			"00000000"
			"\"libtao-dev\""
			"\"5.6.3\"";

		is_null(res_package_unpack("<invalid packed data>"), "res_package_unpack handles bad data");

		isnt_null(r = res_package_unpack(packed), "res_package_unpack succeeds");
		is_string(r->key,     "pkgkey",     "unpacked package key");
		is_string(r->name,    "libtao-dev", "unpacked package name");
		is_string(r->version, "5.6.3",      "unpacked package version");
		res_package_free(r);

		packed = "res_package::\"pkgkey\""
			"00000000"
			"\"libtao-dev\""
			"\"\"";

		isnt_null(r = res_package_unpack(packed), "res_package_unpack succeeds");
		is_null(r->version, "translate empty version string into NULL version");
		res_package_free(r);
	}

	subtest {
		struct res_package *r;
		struct hash *h;

		h = hash_new();
		r = res_package_new("pkg");

		ok(res_package_attrs(r, h) == 0, "got package attrs");
		is_string(hash_get(h, "name"),      "pkg", "h.name");
		is_string(hash_get(h, "installed"), "yes", "h.installed"); // default
		is_null(hash_get(h, "version"), "h.version is unset");

		res_package_set(r, "name", "extra-tools");
		res_package_set(r, "version", "1.2.3-4.5.6");
		res_package_set(r, "installed", "no");

		ok(res_package_attrs(r, h) == 0, "got package attrs");
		is_string(hash_get(h, "name"),      "extra-tools", "h.name");
		is_string(hash_get(h, "version"),   "1.2.3-4.5.6", "h.version");
		is_string(hash_get(h, "installed"), "no",          "h.installed");

		ok(res_package_set(r, "xyzzy", "BAD") != 0, "xyzzy is a bad attr");
		ok(res_package_attrs(r, h) == 0, "got package attrs");
		is_null(hash_get(h, "xyzzy"), "h.xyzzy is unset (bad attr)");

		hash_free_all(h);
		res_package_free(r);
	}

	done_testing();
}
