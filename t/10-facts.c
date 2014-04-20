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
#include "../src/policy.h"
#include "../src/mem.h"

TESTS {
	struct hash *facts;
	FILE *io;

	ok(facts = hash_new(), "allocated a new facts hash");
	fact_parse("sys.kernel.version=2.6.32-194.distro5-generic\n", facts);
	is_string(
		hash_get(facts, "sys.kernel.version"),
		"2.6.32-194.distro5-generic",
		"parse fact line");
	hash_free_all(facts);

	/**********************************************************/

	io = fopen(TEST_DATA "/facts/good.facts", "r");
	ok(io, "good.facts file opened successfully");
	facts = fact_read(io, NULL);
	ok(facts, "fact_read() returns a good pointer");
	is_string(
		hash_get(facts, "test.fact1"),
		"fact1",
		"get test.fact1");
	is_string(
		hash_get(facts, "test.fact2"),
		"fact2",
		"get test.fact2");
	is_string(
		hash_get(facts, "test.multi.level.fact"),
		"multilevel fact",
		"get test.multi.level.fact");
	fclose(io);
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(facts, "hash_new() != NULL");
	io = fopen(TEST_DATA "/facts/good.facts", "r");
	ok(io, "Read good.facts");

	hash_set(facts, "test.fact1", "OVERRIDE ME");
	hash_set(facts, "test.fact2", "OVERRIDE ME");

	ok(fact_read(io, facts), "fact_read(good.facts)");
	is_string(
		hash_get(facts, "test.fact1"), "fact1",
		"get test.fact1");
	is_string(
		hash_get(facts, "test.fact2"), "fact2",
		"get test.fact2");
	is_string(
		hash_get(facts, "test.multi.level.fact"), "multilevel fact",
		"get test.multi.level.fact");
	fclose(io);
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_cat_read(TEST_DATA "/facts/good.facts", facts) == 0,
			"read facts from a file");
	is_string(
		hash_get(facts, "test.fact1"), "fact1",
		"get test.fact1");
	is_string(
		hash_get(facts, "test.fact2"), "fact2",
		"get test.fact2");
	is_string(
		hash_get(facts, "test.multi.level.fact"), "multilevel fact",
		"get test.multi.level.fact");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_gather(TEST_DATA "/facts/gather.d/*.facts", facts) == 0,
			"gather facts from a directory of exec-scripts");
	is_string(
		hash_get(facts, "sys.hostname"), "host22",
		"sys.hostname is defined (from node.facts)");
	is_string(
		hash_get(facts, "sys.os"), "linux",
		"sys.os is defined (from os.facts)");
	ok(!hash_get(facts, "not.defined"),
		"not.defined fact (in skip.me) not defined");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_gather(TEST_DATA "/facts/gather.d/os.facts", facts) == 0,
			"gather facts from a single file (no glob match)");
	is_string(
		hash_get(facts, "sys.os"), "linux",
		"sys.os is defined (from os.facts)");
	ok(!hash_get(facts, "not.defined"),
		"not.defined fact (in skip.me) not defined");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_gather(TEST_DATA "/facts/gather.d/*.nomatch", facts) != 0,
			"failed to gather facts from bad glob match");

	/**********************************************************/

	facts = hash_new();
	hash_set(facts, "test.os",     "Ubuntu");
	hash_set(facts, "test.kernel", "2.6");
	hash_set(facts, "sys.test",    "test-mode");

	sys("mkdir -p " TEST_TMP "");
	sys("rm -f " TEST_TMP "/write.facts");
	io = fopen(TEST_TMP "/write.facts", "w");
	ok(io, "open " TEST_TMP "/write.facts for writing");
	ok(fact_write(io, facts) == 0, "wrote facts");
	fclose(io);

	hash_free(facts); /* don't use hash_free_all; we called hash_set
			     with constant strings. */
	io = fopen(TEST_TMP "/write.facts", "r");
	ok(io, "reopened write.facts for reading");
	facts = hash_new();
	ok(fact_read(io, facts), "fact_read()");
	fclose(io);
	is_string(
		hash_get(facts, "test.os"), "Ubuntu",
		"check test.os");
	is_string(
		hash_get(facts, "test.kernel"), "2.6",
		"check test.kernel");
	is_string(
		hash_get(facts, "sys.test"), "test-mode",
		"check sys.test");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_exec_read(TEST_DATA "/facts/exec.sh", facts) == 0,
			"read facts via exec");
	is_string(
		hash_get(facts, "exec.fact"), "Value",
		"check exec.fact");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_exec_read(TEST_DATA "/facts/ENOENT", facts) != 0,
			"failed to read facts from a non-existent gatherer script");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_exec_read(TEST_DATA "/facts/empty.sh", facts) != 0,
			"failed to read facts from an empty gatherer script");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_exec_read(TEST_DATA "/facts/non-exec.sh", facts) != 0,
			"failed to read facts from a non-executable gatherer script");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_gather(TEST_DATA "/facts/gather.d/*", facts) == 0,
			"gather facts from a directory of exec-scripts");
	is_string(
		hash_get(facts, "sys.hostname"), "host22",
		"sys.hostname is defined (from node.facts)");
	hash_free_all(facts);

	done_testing();
}
