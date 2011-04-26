#include "test.h"
#include "assertions.h"
#include "../pack.h"

void test_pack_encoding_integers()
{
	char *p;

	test("pack: signed 8bit integer");
	p = pack("", "c", -42);
	/* -42 is binary 1101 0110, decimal 214 and hex d6 */
	assert_str_equals("packed representation of -42", "d6", p);
	free(p);

	test("pack: unsigned 8bit integer");
	p = pack("", "C", 125);
	assert_str_equals("packed representation of 125", "7d", p);
	free(p);

	test("pack: signed 16bit integer");
	p = pack("", "s", -21456);
	/* -21456 is 1010 110 0011 0000, decimal 44080, hex ac30 */
	assert_str_equals("packed representation of -21456", "ac30", p);
	free(p);

	test("pack: unsigned 16bit integer");
	p = pack("", "S", 1984);
	assert_str_equals("packed representation of 1984", "07c0", p);
	free(p);

	test("pack: signed 32bit integer");
	p = pack("", "l", -787856564);
	/* -787856564 is binary 1101 0001 0000 1010 0100 001 0100 1100,
	                 decimal 3507110732, hex d10a434c */
	assert_str_equals("packed representation of -787856564", "d10a434c", p);
	free(p);

	test("pack: unsigned 32bit integer");
	p = pack("", "L", 8675309); /* Jenny, I've got your number... */
	assert_str_equals("packed representation of 8675309", "00845fed", p);
	free(p);
}

/* "There's no point in seeking a remedy for a thunderbolt" - Syrus, _Maxims_ */
#define TEST_PACK_SYRUS "Remedium frustra est contra fulmen quaerere"

void test_pack_encoding_strings()
{
	const char *original, *expected;
	char *p;

	test("pack: NULL string encoding");
	original = NULL;
	expected = "\"\"";
	p = pack("", "a", original);
	assert_str_equals("packed representation", expected, p);
	free(p);

	test("pack: EMPTY string encoding");
	original = "";
	expected = "\"\"";
	p = pack("", "a", original);
	assert_str_equals("packed representation", expected, p);
	free(p);

	test("pack: non-special string encoding");
	original = TEST_PACK_SYRUS;
	expected = "\"" TEST_PACK_SYRUS "\"";
	p = pack("", "a", original);
	assert_str_equals("packed representation", expected, p);
	free(p);

	test("pack: string with one quote");
	original = "Example with a \" in it";
	expected = "\"Example with a \\\" in it\"";
	p = pack("", "a", original);
	assert_str_equals("packed representation", expected, p);
	free(p);

	test("pack: string with quotes and other escaped characters");
	original = "\"test\"\\t\\tDouble-tab \"plus quotes\"";
	expected = "\"\\\"test\\\"\\t\\tDouble-tab \\\"plus quotes\\\"\"";
	p = pack("", "a", original);
	assert_str_equals("packed representation", expected, p);
	free(p);
}

void test_pack_multiple_strings()
{
	char *p;
	char *str1, *str2, *str3;

	test("pack: Multiple, consecutive string encoding");
	p = pack("", "aaa", "username", "passwd", "/home/directory");
	assert_str_equals("packed representation", "\"username\"\"passwd\"\"/home/directory\"", p);

	test("pack: Multple, consecutive string decoding");
	assert_int_equals("unpack should return 0", 0, unpack(p, "", "aaa", &str1, &str2, &str3));
	assert_str_equals("string 1 should be 'username'", "username", str1);
	assert_str_equals("string 2 should be 'passwd'", "passwd", str2);
	assert_str_equals("string 3 should be '/home/directory'", "/home/directory", str3);

	free(p);
	free(str1);
	free(str2);
	free(str3);
}

void test_pack_decoding_integers()
{
	uint8_t  u8; uint16_t u16; uint32_t u32;
	 int8_t  i8;  int16_t i16;  int32_t i32;

	test("pack: unpack signed 8-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("d6", "", "c", &i8));
	assert_signed_equals("d6 should unpack to signed 8-bit integer -42", -42, i8);

	test("pack: unpack unsigned 8-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("2a", "", "C", &u8));
	assert_unsigned_equals("2a should unpack to unsigned 8-bit integer 42", 42, u8);

	test("pack: unpack signed 16-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("beef", "", "s", &i16));
	assert_signed_equals("beef should unpack to signed 16-bit integer -16657", -16657, i16);

	test("pack: unpack unsigned 16-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("beef", "", "S", &u16));
	assert_unsigned_equals("beef should unpack to unsigned 16-bit integer 48879", 48879, u16);

	test("pack: unpack signed 32-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("deadbeef", "", "l", &i32));
	// 1101 1110 1010 1101 1011 1110 1110 1111
	assert_signed_equals("deadbeef should unpack to signed 32-bit integer -559038737", -559038737, i32);

	test("pack: unpack unsigned 32-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("deadbeef", "", "L", &u32));
	assert_unsigned_equals("deadbeef should unpack to unsigned 32-bit integer 3735928559", 0xdeadbeef, u32);
}

void test_pack_decoding_strings()
{
	char *buf = NULL;
	const char *packed, *expected;

	test("pack: NULL/empty string decoding");
	packed   = "\"\"";
	expected = "";
	assert_int_equals("unpack should return 0", 0, unpack(packed, "", "a", &buf));
	assert_str_equals("unpacked string", expected, buf);
	free(buf); buf = NULL;

	test("pack: non-special string encoding");
	packed   = "\"" TEST_PACK_SYRUS "\"";
	expected = TEST_PACK_SYRUS;
	assert_int_equals("unpack should return 0", 0, unpack(packed, "", "a", &buf));
	assert_str_equals("unpacked string", expected, buf);
	free(buf); buf = NULL;

	test("pack: string with one quote");
	expected = "Example with a \" in it";
	packed   = "\"Example with a \\\" in it\"";
	assert_int_equals("unpack should return 0", 0, unpack(packed, "", "a", &buf));
	assert_str_equals("unpacked string", expected, buf);
	free(buf); buf = NULL;

	test("pack: string with quotes and other escaped characters");
	packed   = "\"\\\"test\\\"\\t\\tDouble-tab \\\"plus quotes\\\"\"";
	expected = "\"test\"\\t\\tDouble-tab \"plus quotes\"";
	assert_int_equals("unpack should return 0", 0, unpack(packed, "", "a", &buf));
	assert_str_equals("unpacked string", expected, buf);
	free(buf); buf = NULL;
}

void test_pack_DECAFBAD()
{
	char *p;

	/* 0xDECAFBAD can be encoded using unsigned
	   integers of different widths:

	     4 8-bit  -- 222, 202, 251, 173
	     2 16-bit -- 57034, 64429
	     1 32-bit -- 3737844653

	     Other combinations of integer types also work:
	     222, 202, 64429 - 2 8-bit and 1 16-bit
	     57034 251 173   - 1 16-bit and 2 8-bit
	 */
	test("pack: Packing 0xDECAFBAD as 4 unsigned 8-bit integers");
	p = pack("", "CCCC", 222, 202, 251, 173);
	assert_str_equals("packed representation of 222 202 251 173", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD as 2 unsigned 16-bit integers");
	p = pack("", "SS", 57034, 64429);
	assert_str_equals("packed representation of 57034 64429", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD as 1 unsigned 32-bit integers");
	p = pack("", "L", 3737844653UL);
	assert_str_equals("packed representation of 3737844653", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD as 2 unsigned 8-bit and 1 unsigned 16-bit integers");
	p = pack("", "CCS", 222, 202, 64429);
	assert_str_equals("packed representation of 222 202 64429", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD as 1 unsigned 16-bit and 2 unsigned 8-bit integers");
	p = pack("", "SCC", 57034, 251, 173);
	assert_str_equals("packed representation of 57034 251 173", "decafbad", p);
	free(p);

	/* 0xDECAFBAD can also be encoded with signed integers,
	   using the same binary representation, but translating the
	   bit patterns as a two's complement negative number:

	   4 8-bit  -- -34, -54, -5, -83
	   2 16-bit -- -8502, -1107
	   1 32-bit -- -557122643

	   The same rule about combinations of different widths
	   also holds:

	   -34, -54, -1107 - 2 8-bit and 1 16-bit
	   -8502, -5, -83  - 1 16-bit and 2 8-bit
	 */
	test("pack: Packing 0xDECAFBAD as 4 signed 8-bit integers");
	p = pack("", "cccc", -34, -54, -5, -83);
	assert_str_equals("packed representation of -34 -54 -5 -83", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD as 2 signed 16-bit integers");
	p = pack("", "ss", -8502, -1107);
	assert_str_equals("packed representation of -8502 -1107", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD as 1 signed 32-bit integers");
	p = pack("", "l", 0xDECAFBAD);
	assert_str_equals("packed representation of 3737844653", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD as 2 signed 8-bit and 1 signed 16-bit integers");
	p = pack("", "ccs", -34, -54, -1107);
	assert_str_equals("packed representation of -34 -54 -1107", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD as 1 signed 16-bit and 2 signed 8-bit integers");
	p = pack("", "scc", -8502, -5, -83);
	assert_str_equals("packed representation of -8502 -5 -83", "decafbad", p);
	free(p);

	/* Mixing of signed and unsigned types works as well.
	   Here are a few, for illustration.

	   -34 202 -5 173
	   -8502 251 173

	   The remainder of the tests exercise mixed sign packing
	 */
	test("pack: Packing 0xDECAFBAD by alternating signed 8-bit and unsigned 8-bit");
	p = pack("", "cCcC", -34, 202, -5, 173);
	assert_str_equals("packed representation of -34 202, -5, 173", "decafbad", p);
	free(p);

	test("pack: Packing 0xDECAFBAD by with 1 signed 16-bit and 2 unsigned 8-bit integers");
	p = pack("", "sCC", -8502, 251, 173);
	assert_str_equals("packed representation of -8502 251 173", "decafbad", p);
	free(p);
}

void test_pack_interpretation()
{
	uint8_t u8a, u8b, u8c, u8d;
	 int8_t i8a, i8b, i8c, i8d;

	uint16_t u16a, u16b;
	 int16_t i16a, i16b;

	/* 0xDECAFBAD can be encoded using unsigned
	   integers of different widths:

	     4 8-bit  -- 222, 202, 251, 173
	     2 16-bit -- 57034, 64429

	   Other combinations of integer types also work:

	     222, 202, 64429 - 2 8-bit and 1 16-bit
	     57034 251 173   - 1 16-bit and 2 8-bit
	 */

	u8a = u8b = u8c = u8d = u16a = u16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 4 unsigned 8-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "", "CCCC", &u8a, &u8b, &u8c, &u8d));
	assert_unsigned_equals("u8a should be 222", 222, u8a);
	assert_unsigned_equals("u8b should be 202", 202, u8b);
	assert_unsigned_equals("u8c should be 251", 251, u8c);
	assert_unsigned_equals("u8d should be 173", 173, u8d);

	u8a = u8b = u8c = u8d = u16a = u16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 2 unsigned 16-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "", "SS", &u16a, &u16b));
	assert_unsigned_equals("u16a should be 57034", 57034, u16a);
	assert_unsigned_equals("u16b should be 64429", 64429, u16b);

	u8a = u8b = u8c = u8d = u16a = u16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 2 unsigned 8-bit and 1 unsigned 16-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "", "CCS", &u8a, &u8b, &u16b));
	assert_unsigned_equals("u8a should be 222", 222, u8a);
	assert_unsigned_equals("u8b should be 202", 202, u8b);
	assert_unsigned_equals("u16b should be 64429", 64429, u16b);

	u8a = u8b = u8c = u8d = u16a = u16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 1 unsigned 16-bit and 2 unsigned 8-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "", "SCC", &u16a, &u8a, &u8b));
	assert_unsigned_equals("u16b should be 57034", 57034, u16a);
	assert_unsigned_equals("u8a should be 251", 251, u8a);
	assert_unsigned_equals("u8b should be 173", 173, u8b);

	/* Negative numbers (really, two's complement interpretation
	   of the unsigned values; bit patterns) also work:

	     4 8-bit  -- -34, -54, -5, -83
	     2 16-bit -- -8502, -1107

	   The same rule about combinations of different widths
	   also holds:

	     -34, -54, -1107 - 2 8-bit and 1 16-bit
	     -8502, -5, -83  - 1 16-bit and 2 8-bit
	 */
	i8a = i8b = i8c = i8d = i16a = i16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 4 signed 8-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "", "CCCC", &i8a, &i8b, &i8c, &i8d));
	assert_signed_equals("i8a should be -34", -34, i8a);
	assert_signed_equals("i8b should be -54", -54, i8b);
	assert_signed_equals("i8c should be -5", -5, i8c);
	assert_signed_equals("i8d should be -83", -83, i8d);

	i8a = i8b = i8c = i8d = i16a = i16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 2 signed 16-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "", "SS", &i16a, &i16b));
	assert_signed_equals("i16a should be -8502", -8502, i16a);
	assert_signed_equals("i16b should be -1107", -1107, i16b);

	i8a = i8b = i8c = i8d = i16a = i16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 2 signed 8-bit and 1 signed 16-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "", "CCS", &i8a, &i8b, &i16b));
	assert_signed_equals("i8a should be -34", -34, i8a);
	assert_signed_equals("i8b should be -54", -54, i8b);
	assert_signed_equals("i16b should be -1107", -1107, i16b);

	i8a = i8b = i8c = i8d = i16a = i16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 1 signed 16-bit and 2 signed 8-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "", "SCC", &i16a, &i8a, &i8b));
	assert_signed_equals("i16b should be -8502", -8502, i16a);
	assert_signed_equals("i8a should be -5", -5, i8a);
	assert_signed_equals("i8b should be -83", -83, i8b);


	/* Mixing of signed and unsigned types works as well.
	   Here are a few, for illustration.

	     -34 202 -5 173
	     -8502 251 173

	 */


}

void test_pack_failure()
{
	char *s1, *s2, *s3;

	test("pack: Unpacking malformed data");
	assert_int_not_equals("unpack fails with bad prefix", 0,
		unpack("\"s1\"\"s2\"\"s3\"", "prefix::", "aaa",
	               &s1, &s2, &s3));

	/** Not currently working
	assert_int_not_equals("unpack fails with bad data", 0,
		unpack("prefix::<invalid data>", "prefix::", "aaa",
	               &s1, &s2, &s3));
	 **/
}

void test_suite_pack() {
	test_pack_encoding_integers();
	test_pack_encoding_strings();
	test_pack_multiple_strings();

	test_pack_decoding_integers();
	test_pack_decoding_strings();

	test_pack_DECAFBAD();
	test_pack_interpretation();

	test_pack_failure();
}
