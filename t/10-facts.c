#include "test.h"
#include "../src/policy.h"
#include "../src/mem.h"

int main() {
	test();

	struct hash *facts;
	FILE *io;

	ok_pointer(facts = hash_new(), "allocated a new facts hash");
	fact_parse("sys.kernel.version=2.6.32-194.distro5-generic\n", facts);
	is_string(
		hash_get(facts, "sys.kernel.version"),
		"2.6.32-194.distro5-generic",
		"parse fact line");
	hash_free_all(facts);

	/**********************************************************/

	io = fopen("t/data/facts/good.facts", "r");
	ok_pointer(io, "good.facts file opened successfully");
	facts = fact_read(io, NULL);
	ok_pointer(facts, "fact_read() returns a good pointer");
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
	ok_pointer(facts, "hash_new() != NULL");
	io = fopen("t/data/facts/good.facts", "r");
	ok_pointer(io, "Read good.facts");

	hash_set(facts, "test.fact1", "OVERRIDE ME");
	hash_set(facts, "test.fact2", "OVERRIDE ME");

	ok_pointer(fact_read(io, facts), "fact_read(good.facts)");
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
	ok(fact_cat_read("t/data/facts/good.facts", facts) == 0,
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
	ok(fact_gather("t/data/facts/gather.d/*.facts", facts) == 0,
			"gather facts from a directory of exec-scripts");
	is_string(
		hash_get(facts, "sys.hostname"), "host22",
		"sys.hostname is defined (from node.facts)");
	is_string(
		hash_get(facts, "sys.os"), "linux",
		"sys.os is defined (from os.facts)");
	ok_null(
		hash_get(facts, "not.defined"),
		"not.defined fact (in skip.me) not defined");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_gather("t/data/facts/gather.d/os.facts", facts) == 0,
			"gather facts from a single file (no glob match)");
	is_string(
		hash_get(facts, "sys.os"), "linux",
		"sys.os is defined (from os.facts)");
	ok_null(
		hash_get(facts, "not.defined"),
		"not.defined fact (in skip.me) not defined");
	hash_free_all(facts);

	/**********************************************************/

	facts = hash_new();
	ok(fact_gather("t/data/facts/gather.d/*.nomatch", facts) == -1,
			"failed to gather facts from bad glob match");

	/**********************************************************/

	facts = hash_new();
	hash_set(facts, "test.os",     "Ubuntu");
	hash_set(facts, "test.kernel", "2.6");
	hash_set(facts, "sys.test",    "test-mode");

	sys("mkdir -p t/tmp");
	sys("rm -f t/tmp/write.facts");
	io = fopen("t/tmp/write.facts", "w");
	ok_pointer(io, "open t/tmp/write.facts for writing");
	ok(fact_write(io, facts) == 0, "wrote facts");
	fclose(io);

	hash_free(facts); /* don't use hash_free_all; we called hash_set
			     with constant strings. */
	io = fopen("t/tmp/write.facts", "r");
	ok_pointer(io, "reopened write.facts for reading");
	facts = hash_new();
	ok_pointer(fact_read(io, facts), "fact_read()");
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
	ok(fact_exec_read("t/data/facts/exec.sh", facts) == 0,
			"read facts via exec");
	is_string(
		hash_get(facts, "exec.fact"), "Value",
		"check exec.fact");
	hash_free_all(facts);

	/**********************************************************/

	done_testing();
}
