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
	hash_t *facts;
	FILE *io;

	ok(facts = vmalloc(sizeof(hash_t)), "allocated a new facts hash");
	fact_parse("sys.kernel.version=2.6.32-194.distro5-generic\n", facts);
	is(hash_get(facts, "sys.kernel.version"), "2.6.32-194.distro5-generic",
		"parse fact line");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	io = temp_file("test.fact1=fact1\n"
	               "test.fact2=fact2\n"
	               "test.multi.level.fact=multilevel fact\n");
	rewind(io);
	facts = fact_read(io, NULL);
	ok(facts, "fact_read() returns a good pointer");
	is(hash_get(facts, "test.fact1"), "fact1", "get test.fact1");
	is(hash_get(facts, "test.fact2"), "fact2", "get test.fact2");
	is(hash_get(facts, "test.multi.level.fact"), "multilevel fact",
		"get test.multi.level.fact");
	fclose(io);
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = vmalloc(sizeof(hash_t));
	ok(facts, "vmalloc(sizeof(hash_t)) != NULL");
	io = temp_file("test.fact1=fact1\n"
	               "test.fact2=fact2\n"
	               "test.multi.level.fact=multilevel fact\n");
	rewind(io);

	hash_set(facts, "test.fact1", "OVERRIDE ME");
	hash_set(facts, "test.fact2", "OVERRIDE ME");

	ok(fact_read(io, facts), "fact_read(good.facts)");
	is(hash_get(facts, "test.fact1"), "fact1",
		"get test.fact1");
	is(hash_get(facts, "test.fact2"), "fact2",
		"get test.fact2");
	is(hash_get(facts, "test.multi.level.fact"), "multilevel fact",
		"get test.multi.level.fact");
	fclose(io);
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = vmalloc(sizeof(hash_t));
	mkdir("t/tmp", 0777);
	put_file("t/tmp/facts.lst", 0644,
	         "test.fact1=fact1\n"
	         "test.fact2=fact2\n"
	         "test.multi.level.fact=multilevel fact\n");

	ok(fact_cat_read("t/tmp/facts.lst", facts) == 0,
			"read facts from a file");
	is(hash_get(facts, "test.fact1"), "fact1", "get test.fact1");
	is(hash_get(facts, "test.fact2"), "fact2", "get test.fact2");
	is(hash_get(facts, "test.multi.level.fact"), "multilevel fact",
		"get test.multi.level.fact");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = vmalloc(sizeof(hash_t));
	mkdir("t/tmp", 0777);
	mkdir("t/tmp/facts.d", 0777);

	put_file("t/tmp/facts.d/deliberate.fail", 0755,
	         "#!/bin/bash\n"
	         "exit 42\n");

	put_file("t/tmp/facts.d/empty.fail", 0755, "");

	put_file("t/tmp/facts.d/node.facts", 0755,
	         "#!/bin/bash\n"
	         "echo \"sys.type=agent\"\n"
	         "echo \"sys.hostname=host22\"\n"
	         "echo \"sys.fqdn=host22.example.com\"\n");

	put_file("t/tmp/facts.d/noexec.fail", 0644, /* not exec! */
	         "#!/bin/bash\n"
	         "echo \"notexec=FAILED\"\n");

	put_file("t/tmp/facts.d/os.facts", 0755,
	         "#!/bin/bash\n\n"
	         "echo \"sys.os=linux\"\n"
	         "echo \"sys.version=3.2\"\n");

	put_file("t/tmp/facts.d/shebang.fail", 0755,
	         "#!/bin/bin/bin/bin/bash\n"
	         "echo \"fail=fails\"\n");

	put_file("t/tmp/facts.d/skip.me", 0755,
	         "#!/bin/bash\n"
	         "echo \"not.defined=error\"\n");


	ok(fact_gather("t/tmp/facts.d/*.facts", facts) == 0,
			"gather facts from a directory of exec-scripts");
	is_string(
		hash_get(facts, "sys.hostname"), "host22",
		"sys.hostname is defined (from node.facts)");
	is_string(
		hash_get(facts, "sys.os"), "linux",
		"sys.os is defined (from os.facts)");
	ok(!hash_get(facts, "not.defined"),
		"not.defined fact (in skip.me) not defined");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = vmalloc(sizeof(hash_t));
	ok(fact_gather("t/tmp/facts.d/os.facts", facts) == 0,
			"gather facts from a single file (no glob match)");
	is_string(
		hash_get(facts, "sys.os"), "linux",
		"sys.os is defined (from os.facts)");
	ok(!hash_get(facts, "not.defined"),
		"not.defined fact (in skip.me) not defined");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = vmalloc(sizeof(hash_t));
	ok(fact_gather("t/tmp/facts.d/*.nomatch", facts) != 0,
			"failed to gather facts from bad glob match");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = vmalloc(sizeof(hash_t));
	hash_set(facts, "test.os",     "Ubuntu");
	hash_set(facts, "test.kernel", "2.6");
	hash_set(facts, "sys.test",    "test-mode");

	io = fopen("t/tmp/write.facts", "w");
	ok(io, "open t/tmp/write.facts for writing");
	ok(fact_write(io, facts) == 0, "wrote facts");
	fclose(io);

	hash_done(facts, 0);
	free(facts);
	io = fopen("t/tmp/write.facts", "r");
	ok(io, "reopened write.facts for reading");
	facts = vmalloc(sizeof(hash_t));
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
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	put_file("t/tmp/facts.sh", 0755,
	         "#!/bin/bash\n\n"
	         "echo \"exec.fact=Value\"\n");

	facts = vmalloc(sizeof(hash_t));
	ok(fact_exec_read("t/tmp/facts.sh", facts) == 0,
			"read facts via exec");
	is_string(
		hash_get(facts, "exec.fact"), "Value",
		"check exec.fact");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = vmalloc(sizeof(hash_t));
	ok(fact_exec_read("t/tmp/ENOENT", facts) != 0,
			"failed to read facts from a non-existent gatherer script");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	put_file("t/tmp/facts.sh", 0755, "");

	facts = vmalloc(sizeof(hash_t));
	ok(fact_exec_read("t/tmp/facts.sh", facts) != 0,
			"failed to read facts from an empty gatherer script");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	put_file("t/tmp/facts.sh", 0644, /* not exec! */
	         "#!/bin/bash\n\n"
	         "echo \"bash.code=good\"\n");

	facts = vmalloc(sizeof(hash_t));
	ok(fact_exec_read("t/tmp/facts.sh", facts) != 0,
			"failed to read facts from a non-executable gatherer script");
	hash_done(facts, 1);
	free(facts);

	/**********************************************************/

	facts = vmalloc(sizeof(hash_t));
	ok(fact_gather("t/tmp/facts.d/*", facts) == 0,
			"gather facts from a directory of exec-scripts");
	is_string(
		hash_get(facts, "sys.hostname"), "host22",
		"sys.hostname is defined (from node.facts)");
	hash_done(facts, 1);
	free(facts);

	done_testing();
}
