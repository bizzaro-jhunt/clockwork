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
		struct res_symlink *symlink;
		char *key;

		isnt_null(symlink = res_symlink_new("symlink-key"), "generated symlink");
		isnt_null(key = res_symlink_key(symlink), "got symlink key");
		is_string(key, "symlink:symlink-key", "symlink key");
		free(key);
		res_symlink_free(symlink);
	}

	subtest {
		res_symlink_free(NULL);
		pass("res_symlink_free(NULL) doesn't segfault");
	}

	subtest {
		struct res_symlink *r;

		isnt_null(r = res_symlink_new("sudoers"), "created a symlink resource");

		res_symlink_set(r, "path", "t/tmp/symlink");
		is_string(r->path, "t/tmp/symlink", "target symlink to enforce");

		res_symlink_free(r);
	}

	subtest {
		struct res_symlink *r;

		r = res_symlink_new("SUDO");
		res_symlink_set(r, "path",   "/etc/sudoers");
		res_symlink_set(r, "target", "/etc/sudoers.real");

		ok(res_symlink_match(r, "path", "/etc/sudoers") == 0, "match path=/etc/sudoers");
		ok(res_symlink_match(r, "path", "/tmp/wrong")   != 0, "!match path=/tmp/wrong");
		ok(res_symlink_match(r, "target", "/etc/sudoers.real") != 0, "'target' is not a match attr");

		res_symlink_free(r);
	}

	subtest {
		struct res_symlink *r;
		hash_t *h;

		isnt_null(h = vmalloc(sizeof(hash_t)), "created hash");
		isnt_null(r = res_symlink_new("/etc/sudoers"), "created symlink resource");

		ok(res_symlink_attrs(r, h) == 0, "got raw symlink attrs");
		is_string(hash_get(h, "path"),    "/etc/sudoers", "h.path");
		is_string(hash_get(h, "present"), "yes",          "h.present"); // default

		res_symlink_set(r, "present", "no");
		res_symlink_set(r, "target", "/srv/tpl/sudo");

		ok(res_symlink_attrs(r, h) == 0, "got raw symlink attrs");
		is_string(hash_get(h, "target"), "/srv/tpl/sudo", "h.target");
		is_string(hash_get(h, "present"),  "no",          "h.present");

		ok(res_symlink_set(r, "xyzzy", "bad") != 0, "xyzzy is not a valid attr");
		ok(res_symlink_attrs(r, h) == 0, "got raw symlink attrs");
		is_null(hash_get(h, "xyzzy"), "h.xyzzy is unset");

		hash_done(h, 1);
		free(h);
		res_symlink_free(r);
	}

	done_testing();
}
