#include "test.h"
#include "assertions.h"
#include "../string.h"

void assert_auto_string(struct string *s, const char *value)
{
	char buf[256];

	snprintf(buf, 256, "String is set to %s", value);
	assert_str_eq(buf, s->raw, value);

	snprintf(buf, 256, "String is %u chars long", strlen(value));
	assert_int_eq(buf, s->len, strlen(value));
}

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

	test("STRING: Interpolation");

	context = hash_new();
	assert_not_null("(test sanity) hash_new must return a valid hash", context);
	if (!context) { return; }

	hash_set(context, "ref1", "this is a reference");
	hash_set(context, "name", "Clockwork");
	hash_set(context, "multi.level.fact", "MULTILEVEL");
	hash_set(context, "kernel_version", "2.6");

	size_t i;
	for (i = 0; tests[i][0]; i++) {
		string_interpolate(buf, 8192, tests[i][0], context);
		assert_str_eq("interpolate(tpl) == str", tests[i][1], buf);
	}

	hash_free(context);
}

void test_string_automatic()
{
	struct string *s = string_new(NULL, 0);

	test("STRING: Automatic strings");
	assert_auto_string(s, "");

	test("STRING: Append another string");
	assert_int_eq("string_append returns 0", string_append(s, "Hello,"), 0);
	assert_auto_string(s, "Hello,");

	test("STRING: Append a character");
	assert_int_eq("string_append1 returns 0", string_append1(s, ' '), 0);
	assert_auto_string(s, "Hello, ");

	test("STRING: Append a second string");
	assert_int_eq("string_append returns 0", string_append(s, "World!"), 0);
	assert_auto_string(s, "Hello, World!");

	string_free(s);
}

void test_string_extension()
{
	/* Insanely low block size */
	struct string *s = string_new(NULL, 2);

	test("STRING: Buffer extension (2-byte blocks)");
	assert_auto_string(s, "");
	assert_int_eq("s->bytes is 2 (minimum buffer)", s->bytes, 2);

	assert_int_eq("Can add 1 char successfully", string_append1(s, 'a'), 0);
	assert_int_eq("s->bytes is 2 (0+1+NUL)", s->bytes, 2);

	assert_int_eq("Can add 'BBB' successfully", string_append(s, "BBB"), 0);
	assert_auto_string(s, "aBBB");
	assert_int_eq("s->bytes is 6 (0+1+3+NUL)", s->bytes, 6);

	string_free(s);
}

void test_string_initial_value() {
	struct string *s;

	test("STRING: Initial Value");
	s = string_new("Clockwork Rocks Work!", 0);
	assert_auto_string(s, "Clockwork Rocks Work!");

	string_free(s);
}

void test_string_free_null()
{
	struct string *s;

	test("STRING: string_free(NULL)");
	s = NULL; string_free(s);
	assert_null("string_free(NULL) doesn't segfault", s);
}

void test_suite_string()
{
	test_string_interpolation();
	test_string_automatic();
	test_string_extension();
	test_string_initial_value();
	test_string_free_null();
}
