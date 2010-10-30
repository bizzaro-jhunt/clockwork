#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "../mem.h"
#include "../serialize.h"

void test_serialize_string_escape_primitive()
{
	const char *str1 = "this is a regular old string";
	const char *str2 = "";
	const char *str3 = "a \"quoted\" string";

	char *escaped = NULL;

	test("SERIALIZE: escaping strings (primitive)");

	free(escaped);
	escaped = serializer_escape(str1);
	assert_not_null("serializer_escape(str1) returns a string", escaped);
	assert_str_equals("serializer_escape(str1)", "this is a regular old string", escaped);

	free(escaped);
	escaped = serializer_escape(str2);
	assert_not_null("serializer_escape(str2) returns a string", escaped);
	assert_str_equals("serializer_escape(str2)", "", escaped);

	free(escaped);
	escaped = serializer_escape(str3);
	assert_not_null("serializer_escape(str3) returns a string", escaped);
	assert_str_equals("serializer_escape(str3)", "a \\\"quoted\\\" string", escaped);

	free(escaped);
}

void test_serialize_string_unescape_primitive()
{
	const char *str1 = "this is a regular old string";
	const char *str2 = "";
	const char *str3 = "a \\\"quoted\\\" string";
	const char *str4 = "\tTabbed Newline\r\n";
	const char *str5 = "\\tTabbed Newline\\r\\n";
	const char *str6 = "C:\\path\\to\\file";

	char *unescaped = NULL;

	test("SERIALIZE: unescaping strings (primitive)");

	free(unescaped);
	unescaped = unserializer_unescape(str1);
	assert_not_null("unserializer_unescape(str1) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str1)", "this is a regular old string", unescaped);

	free(unescaped);
	unescaped = unserializer_unescape(str2);
	assert_not_null("unserializer_unescape(str2) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str2)", "", unescaped);

	free(unescaped);
	unescaped = unserializer_unescape(str3);
	assert_not_null("unserializer_unescape(str3) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str3)", "a \"quoted\" string", unescaped);

	free(unescaped);
	unescaped = unserializer_unescape(str4);
	assert_not_null("unserializer_unescape(str4) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str4)", "\tTabbed Newline\r\n", unescaped);

	free(unescaped);
	unescaped = unserializer_unescape(str5);
	assert_not_null("unserializer_unescape(str5) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str5)", "\\tTabbed Newline\\r\\n", unescaped);

	free(unescaped);
	unescaped = unserializer_unescape(str6);
	assert_not_null("unserializer_unescape(str6) returns a string", unescaped);
	assert_str_equals("unserializer_unescape(str6)", "C:\\path\\to\\file", unescaped);

	free(unescaped);
}

void test_serialize_serialization()
{
	serializer *s;
	const char *expected = "tagged {\"a string\":\"\":\"-42\":\"42\":\"c\"}";
	char *actual;
	size_t len;

	test("SERIALIZE: Serializer creation");
	s = serializer_new("tagged");
	assert_not_null("serializer_new should return a non-null value", s);

	test("SERIALIZE: Serializing base types");
	assert_int_equals("adding a string works", 0, serializer_add_string(s, "a string"));
	assert_int_equals("adding a NULL string works", 0, serializer_add_string(s, NULL));
	assert_int_equals("adding a negative integer works", 0, serializer_add_int8(s, -42));
	assert_int_equals("adding a positive integer works", 0, serializer_add_uint8(s, 42));
	assert_int_equals("adding a character works", 0, serializer_add_character(s, 'c'));

	assert_int_equals("serializer_finish should return 0", 0, serializer_finish(s));

	assert_int_equals("extracting serialized data", 0, serializer_data(s, &actual, &len));
	assert_int_equals("Length of serialized string", strlen(expected), len);
	assert_str_equals("Serialized string", expected, actual);

	free(actual);
	serializer_free(s);
}

void test_serialize_unserialization()
{
	unserializer *u;
	const char *serialized = "{\"a string\":\"\":\"-42\":\"42\":\"c\"}";

	char *string = NULL;
	size_t len;

	uint8_t u8 = 0;
	int8_t  i8 = 0;
	char c = '\0';

	test("SERIALIZE: Unserializer creation");
	u = unserializer_new(serialized, strlen(serialized));
	assert_not_null("unserializer_new should return a non-null value", u);

	assert_int_equals("unserialize_next_string should return 0", 0, unserializer_next_string(u, &string, &len));
	assert_int_equals("length of token[0] should be 8", 8, len);
	assert_str_equals("token[0] should be 'a string'", "a string", string);
	xfree(string);

	assert_int_equals("unserialize_next_string should return 0", 0, unserializer_next_string(u, &string, &len));
	assert_int_equals("length of token[1] should be 0", 0, len);
	assert_str_equals("token[1] should be ''", "", string);
	xfree(string);

	assert_int_equals("unserialize_next_int8 should return 0", 0, unserializer_next_int8(u, &i8));
	assert_int_equals("token[2] should be -42", -42, i8);

	assert_int_equals("unserialize_next_uint8 should return 0", 0, unserializer_next_uint8(u, &u8));
	assert_int_equals("token[3] should be 42", 42, u8);

	assert_int_equals("unserialize_next_character should return 0", 0, unserializer_next_character(u, &c));
	assert_int_equals("token[4] should be 'c'", 'c', c);

	assert_int_not_equal("unserialize_next_string should not return 0", 0, unserializer_next_string(u, &string, &len));

	unserializer_free(u);
}

/* Warning: this macro really only makes sense in test_serialize_int_uint_sizes() */
#define test_serializer_numeric_bounds(u,from,to,sign,var,min,max) do { \
	(var) = 42; /* non-min, non-max value */ \
	assert_int_equals("unserializer_next_" #to " returns 0", 0, unserializer_next_ ## to ((u), &(var))); \
	assert_ ## sign ## _equals("min(" #from "_t) -> " #to "_t", (min), (var));\
	(var) = 42; /* non-min, non-max value */ \
	assert_int_equals("unserializer_next_" #to " returns 0", 0, unserializer_next_ ## to ((u), &(var))); \
	assert_ ## sign ## _equals("max(" #from "_t) -> " #to "_t", (max), (var));\
} while(0)

void test_serialize_int_uint_sizes()
{
	uint8_t  u8,  u8_min  = 0U, u8_max  = 255U;
	uint16_t u16, u16_min = 0U, u16_max = 65535U;
	uint32_t u32, u32_min = 0U, u32_max = 4294967295U;

	int8_t   i8,  i8_min  = -128,   i8_max  = 127;
	int16_t  i16, i16_min = -32768, i16_max = 32767;
	/* to avoid 'warning: this decimal constant is unsigned only in ISO C90',
	   we have to dance around the minimum value of a 32-bit signed integer,
	   by subtracting one from the almost-minimum value.

	   Yeah, I don't really get it either.  Need to brush up on my
	   C standards.
	 */
	int32_t  i32, i32_min = -2147483647 - 1, i32_max = 2147483647;

	serializer *s;
	unserializer *u;

	char *serialized;
	size_t len;

	test("serialize: unsigned integer tests setup");
	s = serializer_new("uints");
	serializer_add_uint8(s, u8_min);
	serializer_add_uint8(s, u8_max);
	serializer_add_uint16(s, u16_min);
	serializer_add_uint16(s, u16_max);
	serializer_add_uint32(s, u32_min);
	serializer_add_uint32(s, u32_max);
	serializer_finish(s);
	serializer_data(s, &serialized, &len);
	serializer_free(s);

	assert_str_equals("serialization worked (test sanity test)",
		"uints {\"0\":\"255\":\"0\":\"65535\":\"0\":\"4294967295\"}",
		serialized);

	test("serialize: unsigned 8-bit integers");
	u = unserializer_new(serialized, len);
	test_serializer_numeric_bounds(u, uint8,  uint8,  unsigned, u8,  u8_min,  u8_max);
	test_serializer_numeric_bounds(u, uint16, uint8,  unsigned, u8,  u8_min,  u8_max);
	test_serializer_numeric_bounds(u, uint32, uint8,  unsigned, u8,  u8_min,  u8_max);
	unserializer_free(u);

	test("serialize: unsigned 16-bit integers");
	u = unserializer_new(serialized, len);
	test_serializer_numeric_bounds(u, uint8,  uint16, unsigned, u16, u8_min,  u8_max);
	test_serializer_numeric_bounds(u, uint16, uint16, unsigned, u16, u16_min, u16_max);
	test_serializer_numeric_bounds(u, uint32, uint16, unsigned, u16, u16_min, u16_max);
	unserializer_free(u);

	test("serialize: unsigned 32-bit integers");
	u = unserializer_new(serialized, len);
	test_serializer_numeric_bounds(u, uint8,  uint32, unsigned, u32, u8_min,  u8_max);
	test_serializer_numeric_bounds(u, uint16, uint32, unsigned, u32, u16_min, u16_max);
	test_serializer_numeric_bounds(u, uint32, uint32, unsigned, u32, u32_min, u32_max);
	unserializer_free(u);
	free(serialized);

	test("serialize: signed integer tests setup");
	s = serializer_new("ints");
	serializer_add_int8(s, i8_min);
	serializer_add_int8(s, i8_max);
	serializer_add_int16(s, i16_min);
	serializer_add_int16(s, i16_max);
	serializer_add_int32(s, i32_min);
	serializer_add_int32(s, i32_max);
	serializer_finish(s);
	serializer_data(s, &serialized, &len);
	serializer_free(s);

	assert_str_equals("serialization worked (test sanity test)",
		"ints {\"-128\":\"127\":\"-32768\":\"32767\":\"-2147483648\":\"2147483647\"}",
		serialized);

	test("serialize: signed 8-bit integers");
	u = unserializer_new(serialized, len);
	test_serializer_numeric_bounds(u, int8,  int8,  signed, i8,  i8_min,  i8_max);
	test_serializer_numeric_bounds(u, int16, int8,  signed, i8,  i8_min,  i8_max);
	test_serializer_numeric_bounds(u, int32, int8,  signed, i8,  i8_min,  i8_max);
	unserializer_free(u);

	test("serialize: signed 16-bit integers");
	u = unserializer_new(serialized, len);
	test_serializer_numeric_bounds(u, int8,  int16, signed, i16, i8_min,  i8_max);
	test_serializer_numeric_bounds(u, int16, int16, signed, i16, i16_min, i16_max);
	test_serializer_numeric_bounds(u, int32, int16, signed, i16, i16_min, i16_max);
	unserializer_free(u);

	test("serialize: signed 32-bit integers");
	u = unserializer_new(serialized, len);
	test_serializer_numeric_bounds(u, int8,  int32, signed, i32, i8_min,  i8_max);
	test_serializer_numeric_bounds(u, int16, int32, signed, i32, i16_min, i16_max);
	test_serializer_numeric_bounds(u, int32, int32, signed, i32, i32_min, i32_max);
	unserializer_free(u);
	free(serialized);
}

void test_serialize_magic_characters()
{
	serializer *s;
	unserializer *u;

	const char *expected = "magic {\"a \\\"quoted\\\" string\"}";
	char *actual = NULL, *token = NULL;
	size_t len;

	test("SERIALIZE: Escaping double-quote field delimiter during serialization");
	s = serializer_new("magic");
	assert_not_null("serializer_new should return a non-null value", s);

	assert_int_equals("adding quoted string works", 0, serializer_add_string(s, "a \"quoted\" string"));
	assert_int_equals("serializer_finish should return 0", 0, serializer_finish(s));

	xfree(actual);
	assert_int_equals("extracting serialized data", 0, serializer_data(s, &actual, &len));
	assert_int_equals("Length of serialized string", strlen(expected), len);
	assert_str_equals("Serialized string", expected, actual);

	test("SERIALIZE: Un-escaping double-quote field delimiter during unserialization");
	u = unserializer_new(actual, len);
	assert_not_null("unserializer_new should return a non-null value", u);

	xfree(token);
	assert_int_equals("unserialize_next_string should return 0", 0, unserializer_next_string(u, &token, &len));
	assert_int_equals("length of token[0] should be 17", 17, len);
	assert_str_equals("token[0] should be 'a \"quoted\" string'", "a \"quoted\" string", token);

	xfree(token);
	assert_int_not_equal("unserialize_next_string should not return 0", 0, unserializer_next_string(u, &token, &len));

	xfree(actual);
	xfree(token);
	serializer_free(s);
	unserializer_free(u);
}

void test_suite_serialize() {
	test_serialize_string_escape_primitive();
	test_serialize_string_unescape_primitive();

	test_serialize_serialization();
	test_serialize_unserialization();

	test_serialize_int_uint_sizes();

	test_serialize_magic_characters();
}
