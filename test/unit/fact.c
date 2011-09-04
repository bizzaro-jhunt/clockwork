/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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
#include "assertions.h"
#include "../../policy.h"
#include "../../mem.h"

void test_fact_parsing()
{
	struct hash *h;

	test("fact: parsing a string fact");
	h = hash_new();
	assert_not_null("facts hash allocated", h);
	fact_parse("sys.kernel.version=2.6.32-194.distro5-generic\n", h);
	assert_str_eq("fact parsed correctly", hash_get(h, "sys.kernel.version"), "2.6.32-194.distro5-generic");

	hash_free(h);
}

void test_fact_read_io()
{
	struct hash *facts;
	FILE *io;

	test("fact: reading facts from a FILE*");
	io = fopen(TEST_UNIT_DATA "/facts/good.facts", "r");
	assert_not_null("(test sanity) good.facts file opened successfully", io);

	facts = fact_read(io, NULL);
	assert_not_null("fact_read() succeeds", facts);

	assert_str_eq("Checking test.fact1", "fact1", hash_get(facts, "test.fact1"));
	assert_str_eq("Checking test.fact2", "fact2", hash_get(facts, "test.fact2"));
	assert_str_eq("Checking test.multi.level.fact", "multilevel fact", hash_get(facts, "test.multi.level.fact"));
	fclose(io);

	/* Because hashes only do memory management for their keys,
	   we have to manually free the values before calling hash_free */
	free(hash_get(facts, "test.fact1"));
	free(hash_get(facts, "test.fact2"));
	free(hash_get(facts, "test.multi.level.fact"));

	hash_free(facts);
}

void test_fact_read_overrides()
{
	struct hash *facts;
	FILE *io;

	test("fact: reading facts from a FILE* (overrides)");
	facts = hash_new();
	assert_not_null("Pre-read fact hash is valid pointer", facts);
	io = fopen(TEST_UNIT_DATA "/facts/good.facts", "r");
	assert_not_null("(test sanity) good.facts file opened successfully", io);

	hash_set(facts, "test.fact1", "OVERRIDE ME");
	hash_set(facts, "test.fact2", "OVERRIDE ME");
	assert_not_null("fact_read() succeeds", fact_read(io, facts));

	assert_str_eq("Checking test.fact1", "fact1", hash_get(facts, "test.fact1"));
	assert_str_eq("Checking test.fact2", "fact2", hash_get(facts, "test.fact2"));
	assert_str_eq("Checking test.multi.level.fact", "multilevel fact", hash_get(facts, "test.multi.level.fact"));
	fclose(io);

	hash_free_all(facts);
}

void test_fact_write()
{
	struct hash *facts;
	FILE *io;
	const char *path = TEST_UNIT_TEMP "/facts/write.facts";

	test("fact: write facts to a FILE*");
	facts = hash_new();
	hash_set(facts, "test.os",      "Ubuntu");
	hash_set(facts, "test.kernel", "2.6");
	hash_set(facts, "sys.test",     "test-mode");

	io = fopen(path, "w");
	assert_not_null("(test sanity) write.facts opened for writing", io);

	assert_int_eq("fact_write succeeds", fact_write(io, facts), 0);
	fclose(io);
	hash_free(facts); /* don't use hash_free_all; we called hash_set
			     with constant strings. */

	io = fopen(path, "r");
	assert_not_null("(test sanity) write.facts opened for re-reading", io);
	facts = hash_new();
	assert_not_null("fact_read() succeeds", fact_read(io, facts));
	fclose(io);
	assert_str_eq("Checking test.os",     hash_get(facts, "test.os"),     "Ubuntu");
	assert_str_eq("Checking test.kernel", hash_get(facts, "test.kernel"), "2.6");
	assert_str_eq("Checking sys.test",    hash_get(facts, "sys.test"),    "test-mode");

	hash_free_all(facts);
}

void test_suite_fact()
{
	test_fact_parsing();
	test_fact_read_io();
	test_fact_read_overrides();
	test_fact_write();
}
