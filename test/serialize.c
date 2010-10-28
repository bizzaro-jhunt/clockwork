#include <stdlib.h>

#include "test.h"
#include "../serialize.h"

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

void test_suite_serialize() {
	test_serialize_serialization();
	test_serialize_unserialization();
}
