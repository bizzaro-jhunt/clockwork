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
#include "../src/gear/gear.h"

TESTS {
	ok(H64("test") == H64("test"),  "H64 equivalency");
	ok(H64("test") != H64("other"), "H64 equivalency");

	subtest {
		struct hash *h;
		char *path = strdup("/some/path/some/where");
		char *name = strdup("staff");

		ok(H64("name") != H64("path"), "no H64 collision");

		isnt_null(h = hash_new(), "hash_new -> pointer");

		is_null(hash_get(h, "path"), "can't get 'path' prior to set");
		is_null(hash_get(h, "name"), "can't get 'name' prior to set");

		ok(hash_set(h, "path", path) == path, "set h.path");
		ok(hash_set(h, "name", name) == name, "set h.name");

		is_string(hash_get(h, "path"), path, "get h.path");
		is_string(hash_get(h, "name"), name, "get h.name");

		hash_free(h);
		free(path);
		free(name);
	}

	subtest {
		struct hash *h;
		char *path  = strdup("/some/path/some/where");
		char *group = strdup("staff");

		ok(H64("path") == H64("group"), "H64 collisions");
		isnt_null(h = hash_new(), "hash_new -> pointer");

		is_null(hash_get(h, "path"),  "get h.path (initially null)");
		is_null(hash_get(h, "group"), "get h.group (initially null)");

		ok(hash_set(h, "path",  path)  == path,  "set h.path");
		ok(hash_set(h, "group", group) == group, "set h.group");

		is_string(hash_get(h, "path"),  path,  "get h.path");
		is_string(hash_get(h, "group"), group, "get h.group");

		hash_free(h);
		free(path);
		free(group);
	}

	subtest {
		struct hash *h;
		char *value1 = strdup("value1");
		char *value2 = strdup("value2");

		isnt_null(h = hash_new(), "hash_new -> pointer");

		ok(hash_set(h, "key", value1) == value1, "first hash_set for overrides");
		is_string(hash_get(h, "key"), value1, "hash_get of first value");

		// hash_set returns previous value on override
		ok(hash_set(h, "key", value2) == value1, "second hash_set for overrides");
		is_string(hash_get(h, "key"), value2, "hash_get of second value");

		hash_free_all(h);
	}

	subtest {
		struct hash *h = NULL;

		is_null(hash_get(NULL, "test"), "hash_get NULL hash");

		isnt_null(h = hash_new(), "hash_new -> pointer");
		hash_set(h, "test", "valid");

		is_null(hash_get(h, NULL), "hash_get NULL key");
		hash_free(h);
	}

	subtest {
		struct hash *h = hash_new();
		struct hash_cursor c;
		char *key, *value;

		int saw_promise = 0;
		int saw_snooze  = 0;
		int saw_central = 0;
		int saw_bridge  = 0;

		hash_set(h, "promise", "esimorp");
		hash_set(h, "snooze",  "ezoons");
		hash_set(h, "central", "lartnec");
		hash_set(h, "bridge",  "egdirb");

		for_each_key_value(h, &c, key, value) {
			if (strcmp(key, "promise") == 0) {
				saw_promise = 1;
				is_string(value, "esimorp", "h.promise");

			} else if (strcmp(key, "snooze") == 0) {
				saw_snooze = 1;
				is_string(value, "ezoons", "h.snooze");

			} else if (strcmp(key, "central") == 0) {
				saw_central = 1;
				is_string(value, "lartnec", "h.central");

			} else if (strcmp(key, "bridge") == 0) {
				saw_bridge = 1;
				is_string(value, "egdirb", "h.bridge");

			} else {
				fail("Unexpected value found during for_each_key_value");
			}
		}
		hash_free(h);
	}

	done_testing();
}
