#include "test.h"
#include "assertions.h"
#include "../fact.h"

void test_fact_init_deinit()
{
	struct fact *fact;

	test("fact: initialization");
	fact = fact_new("test.fact", "yes sir");
	assert_not_null("fact_new returns a valid fact pointer", fact);
	if (fact) {
		assert_str_equals("fact->name is 'test.fact'", "test.fact", fact->name);
		assert_str_equals("fact->value is 'yes sir'", "yes sir", fact->value);
	}

	fact_free(fact);
}

void test_fact_parsing()
{
	struct fact *fact;

	test("fact: parsing a string fact");
	fact = fact_parse("sys.kernel.version=2.6.32-194.distro5-generic\n");
	assert_not_null("fact_parse should return a fact pointer", fact);
	assert_str_equals("name parsed correctly", "sys.kernel.version", fact->name);
	assert_str_equals("value parsed correctly", "2.6.32-194.distro5-generic", fact->value);

	fact_free(fact);
}

void test_fact_read_io()
{
	struct list facts;
	FILE *io;
	struct fact *fact, *tmp;
	int i = 0;

	list_init(&facts);

	test("fact: reading facts from a FILE*");
	assert_true("(test sanity) list 'facts' should be empty before we start", list_empty(&facts));
	io = fopen("test/data/facts/good.facts", "r");
	assert_not_null("(test sanity) good.facts file opened successfully", io);

	assert_int_equals("fact_read() returns 3 facts", 3, fact_read(&facts, io));
	for_each_node(fact, &facts, facts) {
		switch (i) {
		case 0:
			assert_not_null("fact[0] shoul not be NULL", fact);
			assert_str_equals("fact[0]->name should be 'test.fact1'", "test.fact1", fact->name);
			assert_str_equals("fact[0]->value should be 'fact1'", "fact1", fact->value);
			break;
		case 1:
			assert_not_null("fact[1] shoul not be NULL", fact);
			assert_str_equals("fact[1]->name should be 'test.fact2'", "test.fact2", fact->name);
			assert_str_equals("fact[1]->value should be 'fact2'", "fact2", fact->value);
			break;
		case 2:
			assert_not_null("fact[2] shoul not be NULL", fact);
			assert_str_equals("fact[2]->name should be 'test.multi.level.fact'", "test.multi.level.fact", fact->name);
			assert_str_equals("fact[2]->value should be 'multilevel fact'", "multilevel fact", fact->value);
			break;
		default:
			assert_fail("Unexpected fact found in the list");
		}

		i++;
	}

	fclose(io);

	/* free all facts */
	for_each_node_safe(fact, tmp, &facts, facts) {
		fact_free(fact);
	}
}

void test_suite_fact()
{
	test_fact_init_deinit();
	test_fact_parsing();
	test_fact_read_io();
}
