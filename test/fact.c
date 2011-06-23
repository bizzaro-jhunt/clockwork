#include "test.h"
#include "assertions.h"
#include "../policy.h"
#include "../mem.h"

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
	io = fopen("test/data/facts/good.facts", "r");
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
	io = fopen("test/data/facts/good.facts", "r");
	assert_not_null("(test sanity) good.facts file opened successfully", io);

	hash_set(facts, "test.fact1", "OVERRIDE ME");
	hash_set(facts, "test.fact2", "OVERRIDE ME");
	assert_not_null("fact_read() succeeds", fact_read(io, facts));

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

void test_suite_fact()
{
	test_fact_parsing();
	test_fact_read_io();
	test_fact_read_overrides();
}
