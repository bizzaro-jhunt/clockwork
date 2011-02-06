#include "test.h"
#include "assertions.h"
#include "../string.h"

void test_string_interpolation()
{
	char buf[8192];
	struct hash *context;

	const char *tests[][2] = {
		{ "string with no references",
		  "string with no references" },

		{ "string with simple (a-z) ref: $ref1",
		  "string with simple (a-z) ref: this is a reference" },

		{ "multi.level.fact = ${multi.level.fact}",
		  "multi.level.fact = MULTILEVEL" },

		{ "Unknown variable: $unknown",
		  "Unknown variable: " },

		{ "Unknown CREF: ${unknown.cref}",
		  "Unknown CREF: " },

		{ "Escaped ref: \\$ref1",
		  "Escaped ref: $ref1" },

		{ "Multiple refs: $ref1, $ref1 and $name",
		  "Multiple refs: this is a reference, this is a reference and Clockwork" },

		{ "Dollar sign at end of string: \\$",
		  "Dollar sign at end of string: $" },

		{ NULL, NULL }
	};

	context = hash_new();
	assert_not_null("(test sanity) hash_new must return a valid hash", context);
	if (!context) { return; }

	hash_set(context, "ref1", "this is a reference");
	hash_set(context, "name", "Clockwork");
	hash_set(context, "multi.level.fact", "MULTILEVEL");
	hash_set(context, "kernel_version", "2.6");

	const char *tpl, *str;
	size_t i;
	for (i = 0; tests[i][0]; i++) {
		string_interpolate(buf, 8192, tests[i][0], context);
		assert_str_equals("interpolate(tpl) == str", tests[i][1], buf);
	}
}

void test_suite_string()
{
	test_string_interpolation();
}
