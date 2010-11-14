#include "test.h"
#include "assertions.h"
#include "../policy.h"
#include "../mem.h"

void test_fact_parsing()
{
	char *name, *value;

	test("fact: parsing a string fact");
	fact_parse("sys.kernel.version=2.6.32-194.distro5-generic\n", &name, &value);
	assert_str_equals("name parsed correctly", "sys.kernel.version", name);
	assert_str_equals("value parsed correctly", "2.6.32-194.distro5-generic", value);

	xfree(name);
	xfree(value);
}

void test_fact_read_io()
{
	struct hash *facts;
	FILE *io;

	test("fact: reading facts from a FILE*");
	io = fopen("test/data/facts/good.facts", "r");
	assert_not_null("(test sanity) good.facts file opened successfully", io);

	facts = fact_read(io);
	assert_not_null("fact_read() succeeds", fact_read(io));

	assert_str_equals("Checking test.fact1", "fact1", hash_lookup(facts, "test.fact1"));
	assert_str_equals("Checking test.fact2", "fact2", hash_lookup(facts, "test.fact2"));
	assert_str_equals("Checking test.multi.level.fact", "multilevel fact", hash_lookup(facts, "test.multi.level.fact"));
	fclose(io);

	hash_free(facts);
}

void test_suite_fact()
{
	test_fact_parsing();
	test_fact_read_io();
}
