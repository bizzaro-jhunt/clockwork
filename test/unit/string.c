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
#include "../../string.h"

void assert_auto_string(struct string *s, const char *value)
{
	char buf[256];

	snprintf(buf, 256, "String is set to %s", value);
	assert_str_eq(buf, s->raw, value);

	snprintf(buf, 256, "String is %u chars long", strlen(value));
	assert_int_eq(buf, s->len, strlen(value));
}

NEW_TEST(string_interpolation)
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

		{ "Bare dollar sign at end: $",
		  "Bare dollar sign at end: " },

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

NEW_TEST(string_interpolate_short_stroke)
{
	char buf[512]; // extra-large
	struct hash *context;

	test("STRING: Interpolation (short-stroke the buffer)");
	context = hash_new();
	hash_set(context, "ref", "1234567890abcdef");

	string_interpolate(buf, 8, "$ref is 16 characters long", context);
	assert_str_eq("interpolated value cut short", buf, "1234567");

	hash_free(context);
}

NEW_TEST(string_automatic)
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

NEW_TEST(string_extension)
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

NEW_TEST(string_initial_value) {
	struct string *s;

	test("STRING: Initial Value");
	s = string_new("Clockwork Rocks Work!", 0);
	assert_auto_string(s, "Clockwork Rocks Work!");

	string_free(s);
}

NEW_TEST(string_free_null)
{
	struct string *s;

	test("STRING: string_free(NULL)");
	s = NULL; string_free(s);
	assert_null("string_free(NULL) doesn't segfault", s);
}

NEW_SUITE(string)
{
	RUN_TEST(string_interpolation);
	RUN_TEST(string_interpolate_short_stroke);
	RUN_TEST(string_automatic);
	RUN_TEST(string_extension);
	RUN_TEST(string_initial_value);
	RUN_TEST(string_free_null);
}
