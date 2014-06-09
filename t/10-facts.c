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

TESTS {
	cw_hash_t *facts;
	FILE *io;

	ok(facts = cw_alloc(sizeof(cw_hash_t)), "allocated a new facts hash");
	fact_parse("sys.kernel.version=2.6.32-194.distro5-generic\n", facts);
	is_string(
		cw_hash_get(facts, "sys.kernel.version"),
		"2.6.32-194.distro5-generic",
		"parse fact line");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	io = fopen(TEST_DATA "/facts/good.facts", "r");
	ok(io, "good.facts file opened successfully");
	facts = fact_read(io, NULL);
	ok(facts, "fact_read() returns a good pointer");
	is_string(
		cw_hash_get(facts, "test.fact1"),
		"fact1",
		"get test.fact1");
	is_string(
		cw_hash_get(facts, "test.fact2"),
		"fact2",
		"get test.fact2");
	is_string(
		cw_hash_get(facts, "test.multi.level.fact"),
		"multilevel fact",
		"get test.multi.level.fact");
	fclose(io);
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(facts, "cw_alloc(sizeof(cw_hash_t)) != NULL");
	io = fopen(TEST_DATA "/facts/good.facts", "r");
	ok(io, "Read good.facts");

	cw_hash_set(facts, "test.fact1", "OVERRIDE ME");
	cw_hash_set(facts, "test.fact2", "OVERRIDE ME");

	ok(fact_read(io, facts), "fact_read(good.facts)");
	is_string(
		cw_hash_get(facts, "test.fact1"), "fact1",
		"get test.fact1");
	is_string(
		cw_hash_get(facts, "test.fact2"), "fact2",
		"get test.fact2");
	is_string(
		cw_hash_get(facts, "test.multi.level.fact"), "multilevel fact",
		"get test.multi.level.fact");
	fclose(io);
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_cat_read(TEST_DATA "/facts/good.facts", facts) == 0,
			"read facts from a file");
	is_string(
		cw_hash_get(facts, "test.fact1"), "fact1",
		"get test.fact1");
	is_string(
		cw_hash_get(facts, "test.fact2"), "fact2",
		"get test.fact2");
	is_string(
		cw_hash_get(facts, "test.multi.level.fact"), "multilevel fact",
		"get test.multi.level.fact");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_gather(TEST_DATA "/facts/gather.d/*.facts", facts) == 0,
			"gather facts from a directory of exec-scripts");
	is_string(
		cw_hash_get(facts, "sys.hostname"), "host22",
		"sys.hostname is defined (from node.facts)");
	is_string(
		cw_hash_get(facts, "sys.os"), "linux",
		"sys.os is defined (from os.facts)");
	ok(!cw_hash_get(facts, "not.defined"),
		"not.defined fact (in skip.me) not defined");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_gather(TEST_DATA "/facts/gather.d/os.facts", facts) == 0,
			"gather facts from a single file (no glob match)");
	is_string(
		cw_hash_get(facts, "sys.os"), "linux",
		"sys.os is defined (from os.facts)");
	ok(!cw_hash_get(facts, "not.defined"),
		"not.defined fact (in skip.me) not defined");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_gather(TEST_DATA "/facts/gather.d/*.nomatch", facts) != 0,
			"failed to gather facts from bad glob match");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	cw_hash_set(facts, "test.os",     "Ubuntu");
	cw_hash_set(facts, "test.kernel", "2.6");
	cw_hash_set(facts, "sys.test",    "test-mode");

	sys("mkdir -p " TEST_TMP "");
	sys("rm -f " TEST_TMP "/write.facts");
	io = fopen(TEST_TMP "/write.facts", "w");
	ok(io, "open " TEST_TMP "/write.facts for writing");
	ok(fact_write(io, facts) == 0, "wrote facts");
	fclose(io);

	cw_hash_done(facts, 0);
	free(facts);
	io = fopen(TEST_TMP "/write.facts", "r");
	ok(io, "reopened write.facts for reading");
	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_read(io, facts), "fact_read()");
	fclose(io);
	is_string(
		cw_hash_get(facts, "test.os"), "Ubuntu",
		"check test.os");
	is_string(
		cw_hash_get(facts, "test.kernel"), "2.6",
		"check test.kernel");
	is_string(
		cw_hash_get(facts, "sys.test"), "test-mode",
		"check sys.test");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_exec_read(TEST_DATA "/facts/exec.sh", facts) == 0,
			"read facts via exec");
	is_string(
		cw_hash_get(facts, "exec.fact"), "Value",
		"check exec.fact");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_exec_read(TEST_DATA "/facts/ENOENT", facts) != 0,
			"failed to read facts from a non-existent gatherer script");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_exec_read(TEST_DATA "/facts/empty.sh", facts) != 0,
			"failed to read facts from an empty gatherer script");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_exec_read(TEST_DATA "/facts/non-exec.sh", facts) != 0,
			"failed to read facts from a non-executable gatherer script");
	cw_hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = cw_alloc(sizeof(cw_hash_t));
	ok(fact_gather(TEST_DATA "/facts/gather.d/*", facts) == 0,
			"gather facts from a directory of exec-scripts");
	is_string(
		cw_hash_get(facts, "sys.hostname"), "host22",
		"sys.hostname is defined (from node.facts)");
	cw_hash_done(facts, 1);
	free(facts);

	done_testing();
}
