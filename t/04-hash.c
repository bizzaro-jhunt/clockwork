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
	subtest {
		cw_hash_t *h;
		char *path = strdup("/some/path/some/where");
		char *name = strdup("staff");

		isnt_null(h = cw_alloc(sizeof(cw_hash_t)), "hash_new -> pointer");

		is_null(cw_hash_get(h, "path"), "can't get 'path' prior to set");
		is_null(cw_hash_get(h, "name"), "can't get 'name' prior to set");

		ok(cw_hash_set(h, "path", path) == path, "set h.path");
		ok(cw_hash_set(h, "name", name) == name, "set h.name");

		is_string(cw_hash_get(h, "path"), path, "get h.path");
		is_string(cw_hash_get(h, "name"), name, "get h.name");

		cw_hash_done(h, 0);
		free(path);
		free(name);
	}

	subtest {
		cw_hash_t *h;
		char *path  = strdup("/some/path/some/where");
		char *group = strdup("staff");

		isnt_null(h = cw_alloc(sizeof(cw_hash_t)), "hash_new -> pointer");

		is_null(cw_hash_get(h, "path"),  "get h.path (initially null)");
		is_null(cw_hash_get(h, "group"), "get h.group (initially null)");

		ok(cw_hash_set(h, "path",  path)  == path,  "set h.path");
		ok(cw_hash_set(h, "group", group) == group, "set h.group");

		is_string(cw_hash_get(h, "path"),  path,  "get h.path");
		is_string(cw_hash_get(h, "group"), group, "get h.group");

		cw_hash_done(h, 0);
		free(path);
		free(group);
	}

	subtest {
		cw_hash_t *h;
		char *value1 = strdup("value1");
		char *value2 = strdup("value2");

		isnt_null(h = cw_alloc(sizeof(cw_hash_t)), "hash_new -> pointer");

		ok(cw_hash_set(h, "key", value1) == value1, "first hash_set for overrides");
		is_string(cw_hash_get(h, "key"), value1, "cw_hash_get of first value");

		// cw_hash_set returns previous value on override
		ok(cw_hash_set(h, "key", value2) == value1, "second hash_set for overrides");
		is_string(cw_hash_get(h, "key"), value2, "cw_hash_get of second value");

		cw_hash_done(h, 1);
	}

	subtest {
		cw_hash_t *h = NULL;

		is_null(cw_hash_get(NULL, "test"), "cw_hash_get NULL hash");

		isnt_null(h = cw_alloc(sizeof(cw_hash_t)), "hash_new -> pointer");
		cw_hash_set(h, "test", "valid");

		is_null(cw_hash_get(h, NULL), "cw_hash_get NULL key");
		cw_hash_done(h, 0);
	}

	subtest {
		cw_hash_t *h = cw_alloc(sizeof(cw_hash_t));
		char *key, *value;

		int saw_promise = 0;
		int saw_snooze  = 0;
		int saw_central = 0;
		int saw_bridge  = 0;

		cw_hash_set(h, "promise", "esimorp");
		cw_hash_set(h, "snooze",  "ezoons");
		cw_hash_set(h, "central", "lartnec");
		cw_hash_set(h, "bridge",  "egdirb");

		for_each_key_value(h, key, value) {
			if (strcmp(key, "promise") == 0) {
				saw_promise++;
				is_string(value, "esimorp", "h.promise");

			} else if (strcmp(key, "snooze") == 0) {
				saw_snooze++;
				is_string(value, "ezoons", "h.snooze");

			} else if (strcmp(key, "central") == 0) {
				saw_central++;
				is_string(value, "lartnec", "h.central");

			} else if (strcmp(key, "bridge") == 0) {
				saw_bridge++;
				is_string(value, "egdirb", "h.bridge");

			} else {
				fail("Unexpected value found during for_each_key_value");
			}
		}
		cw_hash_done(h, 0);

		is_int(saw_promise, 1, "saw the promise key only once");
		is_int(saw_snooze,  1, "saw the snooze key only once");
		is_int(saw_central, 1, "saw the central key only once");
		is_int(saw_bridge,  1, "saw the bridge key only once");
	}

	done_testing();
}
