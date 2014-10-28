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
#include "../src/resource.h"

TESTS {
	subtest {
		resource_free(NULL);
		pass("resource_free(NULL) doesn't segfault");

		dependency_free(NULL);
		pass("dependency_free(NULL) doesn't segfault");
	}

	subtest {
		struct resource *res;
		char *key;

		isnt_null(res = resource_new("user", "user1"),
			"resource_new allocates a resource structure");
		isnt_null(res->resource, "resource_new allocates a res_* member");

		is_string(key = resource_key(res), "user:user1", "resource_key");
		free(key);

		resource_free(res);
	}

	subtest {
		struct resource *res;
		hash_t *h;

		isnt_null(res = resource_new("host", "localhost"), "created host resource");
		resource_set(res, "ip", "127.0.0.1");
		isnt_null(h = resource_attrs(res), "got raw host attrs");
		is_string(hash_get(h, "hostname"), "localhost", "host name attr");
		is_string(hash_get(h, "ip"),       "127.0.0.1", "host ip attr");
		is_null(hash_get(h, "aliases"), "aliases attr is unset");

		hash_done(h, 1);
		free(h);
		resource_free(res);
	}

	subtest {
		struct resource *a, *b, *c;

		isnt_null(a = resource_new("file",  "/path"), "a is a valid resource");
		isnt_null(b = resource_new("user",  "owner"), "b is a valid resource");
		isnt_null(c = resource_new("group", "grp11"), "c is a valid resource");

		ok(resource_depends_on(a, b) != 0, "!(a -> b)");
		ok(resource_depends_on(a, c) != 0, "!(a -> c)");
		ok(resource_depends_on(b, a) != 0, "!(b -> a)");
		ok(resource_depends_on(b, c) != 0, "!(b -> c)");
		ok(resource_depends_on(c, a) != 0, "!(c -> a)");
		ok(resource_depends_on(c, b) != 0, "!(c -> b)");

		ok(resource_add_dependency(a, b) == 0, "make a -> b");
		ok(resource_depends_on(a, b) == 0,   "a -> b");
		ok(resource_depends_on(a, c) != 0, "!(a -> c)");
		ok(resource_depends_on(b, a) != 0, "!(b -> a)");
		ok(resource_depends_on(b, c) != 0, "!(b -> c)");
		ok(resource_depends_on(c, a) != 0, "!(c -> a)");
		ok(resource_depends_on(c, b) != 0, "!(c -> b)");

		ok(resource_add_dependency(a, c) == 0, "make a -> c");
		ok(resource_depends_on(a, b) == 0,   "a -> b");
		ok(resource_depends_on(a, c) == 0,   "a -> c");
		ok(resource_depends_on(b, a) != 0, "!(b -> a)");
		ok(resource_depends_on(b, c) != 0, "!(b -> c)");
		ok(resource_depends_on(c, a) != 0, "!(c -> a)");
		ok(resource_depends_on(c, b) != 0, "!(c -> b)");

		ok(resource_drop_dependency(a, b) == 0, "dropped a -> b dep");
		ok(resource_depends_on(a, b) != 0, "!(a -> b)");
		ok(resource_depends_on(a, c) == 0,   "a -> c");
		ok(resource_depends_on(b, a) != 0, "!(b -> a)");
		ok(resource_depends_on(b, c) != 0, "!(b -> c)");
		ok(resource_depends_on(c, a) != 0, "!(c -> a)");
		ok(resource_depends_on(c, b) != 0, "!(c -> b)");

		ok(resource_drop_dependency(a, c) == 0, "dropped a -> c dep");
		ok(resource_depends_on(a, b) != 0, "!(a -> b)");
		ok(resource_depends_on(a, c) != 0, "!(a -> c)");
		ok(resource_depends_on(b, a) != 0, "!(b -> a)");
		ok(resource_depends_on(b, c) != 0, "!(b -> c)");
		ok(resource_depends_on(c, a) != 0, "!(c -> a)");
		ok(resource_depends_on(c, b) != 0, "!(c -> b)");

		ok(resource_drop_dependency(c, b) != 0, "dropping non-existent deo fails");

		resource_free(a);
		resource_free(b);
		resource_free(c);
	}

	subtest {
		dependency_free(NULL);
		pass("dependency_free(NULL) does not segfault");
	}

	subtest {
		is_null(resource_new("nonexistent_resource", "DNF"),
			"can't create unknown resource types");
	}

	done_testing();
}
