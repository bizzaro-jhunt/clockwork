#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "../serialize.h"

void test_serialize_string_escape_primitive()
{
	const char *str1 = "this is a regular old string";
	const char *str2 = "";
	const char *str3 = "a \"quoted\" string";

	char *escaped;

	test("SERIALIZE: escaping strings (primitive)");
	escaped = serializer_escape(str1);
	assert_not_null("serializer_escape(str1) returns a string", escaped);
	assert_str_equals("serializer_escape(str1)", "this is a regular old string", escaped);

	escaped = serializer_escape(str2);
	assert_not_null("serializer_escape(str2) returns a string", escaped);
	assert_str_equals("serializer_escape(str2)", "", escaped);

	escaped = serializer_escape(str3);
	assert_not_null("serializer_escape(str3) returns a string", escaped);
	assert_str_equals("serializer_escape(str3)", "a \\\"quoted\\\" string", escaped);
}

void test_serialize_string_unescape_primitive()
{
	const char *str1 = "this is a regular old string";
	const char *str2 = "";
	const char *str3 = "a \\\"quoted\\\" string";
	const char *str4 = "\tTabbed Newline\r\n";
	const char *str5 = "\\tTabbed Newline\\r\\n";
	const char *str6 = "C:\\path\\to\\file";

	char *unescaped;

	test("SERIALIZE: unescaping strings (primitive)");
	unescaped = unserializer_unescape(str1);
	assert_not_null("unserializer_unescape(str1) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str1)", "this is a regular old string", unescaped);

	unescaped = unserializer_unescape(str2);
	assert_not_null("unserializer_unescape(str2) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str2)", "", unescaped);

	unescaped = unserializer_unescape(str3);
	assert_not_null("unserializer_unescape(str3) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str3)", "a \"quoted\" string", unescaped);

	unescaped = unserializer_unescape(str4);
	assert_not_null("unserializer_unescape(str4) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str4)", "\tTabbed Newline\r\n", unescaped);

	unescaped = unserializer_unescape(str5);
	assert_not_null("unserializer_unescape(str5) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str5)", "\\tTabbed Newline\\r\\n", unescaped);

	unescaped = unserializer_unescape(str6);
	assert_not_null("unserializer_unescape(str6) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str6)", "C:\\path\\to\\file", unescaped);
}

void test_serialize_serialization()
{
	serializer *s;
	const char *expected = "{\"a string\":\"\":\"-42\":\"42\":\"c\"}";
	char *actual;
	size_t len;

	test("SERIALIZE: Serializer creation");
	s = serializer_new();
	assert_not_null("serializer_new should return a non-null value", s);
	assert_int_equals("serializer_start should return 0", 0, serializer_start(s));

	test("SERIALIZE: Serializing base types");
	assert_int_equals("adding a string works", 0, serializer_add_string(s, "a string"));
	assert_int_equals("adding a NULL string works", 0, serializer_add_string(s, NULL));
	assert_int_equals("adding a negative integer works", 0, serializer_add_signed_integer(s, -42));
	assert_int_equals("adding a positive integer works", 0, serializer_add_unsigned_integer(s, 42));
	assert_int_equals("adding a character works", 0, serializer_add_character(s, 'c'));

	assert_int_equals("serializer_finish should return 0", 0, serializer_finish(s));

	assert_int_equals("extracting serialized data", 0, serializer_data(s, &actual, &len));
	assert_int_equals("Length of serialized string", strlen(expected), len);
	assert_str_equals("Serialized string", expected, actual);

	free(actual);
}

void test_serialize_unserialization()
{
	unserializer *u;
	const char *serialized = "{\"a string\":\"\":\"-42\":\"42\":\"c\"}";
	char *token = strdup("invalid value");
	size_t len;

	test("SERIALIZE: Unserializer creation");
	u = unserializer_new(serialized, strlen(serialized));
	assert_not_null("unserializer_new should return a non-null value", u);
	assert_int_equals("unserializer_start should return 0", 0, unserializer_start(u));

	assert_int_equals("unserialize_get_next should return 0", 0, unserializer_get_next(u, &token, &len));
	assert_int_equals("length of token[0] should be 8", 8, len);
	assert_str_equals("token[0] should be 'a string'", "a string", token);

	assert_int_equals("unserialize_get_next should return 0", 0, unserializer_get_next(u, &token, &len));
	assert_int_equals("length of token[1] should be 0", 0, len);
	assert_str_equals("token[1] should be ''", "", token);

	assert_int_equals("unserialize_get_next should return 0", 0, unserializer_get_next(u, &token, &len));
	assert_int_equals("length of token[2] should be 3", 3, len);
	assert_str_equals("token[2] should be '-42'", "-42", token);

	assert_int_equals("unserialize_get_next should return 0", 0, unserializer_get_next(u, &token, &len));
	assert_int_equals("length of token[3] should be 2", 2, len);
	assert_str_equals("token[3] should be '42'", "42", token);

	assert_int_equals("unserialize_get_next should return 0", 0, unserializer_get_next(u, &token, &len));
	assert_int_equals("length of token[4] should be 1", 1, len);
	assert_str_equals("token[4] should be 'c'", "c", token);

	assert_int_not_equal("unserialize_get_next should not return 0", 0, unserializer_get_next(u, &token, &len));
}

void test_serialize_magic_characters()
{
	serializer *s;
	unserializer *u;

	const char *expected = "{\"a \\\"quoted\\\" string\"}";
	char *actual, *token;
	size_t len;

	test("SERIALIZE: Escaping double-quote field delimiter during serialization");
	s = serializer_new();
	assert_not_null("serializer_new should return a non-null value", s);
	assert_int_equals("serializer_start should return 0", 0, serializer_start(s));

	assert_int_equals("adding quoted string works", 0, serializer_add_string(s, "a \"quoted\" string"));
	assert_int_equals("serializer_finish should return 0", 0, serializer_finish(s));

	assert_int_equals("extracting serialized data", 0, serializer_data(s, &actual, &len));
	assert_int_equals("Length of serialized string", strlen(expected), len);
	assert_str_equals("Serialized string", expected, actual);

	test("SERIALIZE: Un-escaping double-quote field delimiter during unserialization");
	u = unserializer_new(actual, len);
	assert_not_null("unserializer_new should return a non-null value", u);
	assert_int_equals("unserializer_start should return 0", 0, unserializer_start(u));

	assert_int_equals("unserialize_get_next should return 0", 0, unserializer_get_next(u, &actual, &len));
	assert_int_equals("length of token[0] should be 17", 17, len);
	assert_str_equals("token[0] should be 'a \"quoted\" string'", "a \"quoted\" string", actual);

	assert_int_not_equal("unserialize_get_next should not return 0", 0, unserializer_get_next(u, &actual, &len));

	free(actual);
}

void test_suite_serialize() {
	test_serialize_string_escape_primitive();
	test_serialize_string_unescape_primitive();

	test_serialize_serialization();
	test_serialize_unserialization();

	test_serialize_magic_characters();
}
