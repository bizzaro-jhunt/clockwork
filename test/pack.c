#include "test.h"
#include "assertions.h"
#include "../pack.h"

void test_pack_encoding_integers()
{
	size_t nbytes;
	char buf[256];

	test("pack: signed 8bit integer");
	nbytes = pack(buf, sizeof(buf), "c", -42);
	assert_int_equals("pack should return 3 bytes written", 3, nbytes);
	/* -42 is binary 1101 0110, decimal 214 and hex d6 */
	assert_str_equals("packed representation of -42", "d6", buf);

	test("pack: unsigned 8bit integer");
	nbytes = pack(buf, sizeof(buf), "C", 125);
	assert_int_equals("pack should return 3 bytes written", 3, nbytes);
	assert_str_equals("packed representation of 125", "7d", buf);

	test("pack: signed 16bit integer");
	nbytes = pack(buf, sizeof(buf), "s", -21456);
	assert_int_equals("pack should return 5 bytes written", 5, nbytes);
	/* -21456 is 1010 110 0011 0000, decimal 44080, hex ac30 */
	assert_str_equals("packed representation of -21456", "ac30", buf);

	test("pack: unsigned 16bit integer");
	nbytes = pack(buf, sizeof(buf), "S", 1984);
	assert_int_equals("pack should return 5 bytes written", 5, nbytes);
	assert_str_equals("packed representation of 1984", "07c0", buf);

	test("pack: signed 32bit integer");
	nbytes = pack(buf, sizeof(buf), "l", -787856564);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	/* -787856564 is binary 1101 0001 0000 1010 0100 001 0100 1100,
	                 decimal 3507110732, hex d10a434c */
	assert_str_equals("packed representation of -787856564", "d10a434c", buf);

	test("pack: unsigned 32bit integer");
	nbytes = pack(buf, sizeof(buf), "L", 8675309); /* Jenny, I've got your number... */
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of 8675309", "00845fed", buf);
}

/* "There's no point in seeking a remedy for a thunderbolt" - Syrus, _Maxims_ */
#define TEST_PACK_SYRUS "Remedium frustra est contra fulmen quaerere"

void test_pack_encoding_strings()
{
	size_t nbytes;
	char buf[256];
	const char *original, *expected;

	test("pack: NULL string encoding");
	original = NULL;
	expected = "\"\"";
	nbytes = pack(buf, sizeof(buf), "a", original);
	assert_int_equals("pack should return bytes written", strlen(expected)+1, nbytes);
	assert_str_equals("packed representation", expected, buf);

	test("pack: EMPTY string encoding");
	original = "";
	expected = "\"\"";
	nbytes = pack(buf, sizeof(buf), "a", original);
	assert_int_equals("pack should return bytes written", strlen(expected)+1, nbytes);
	assert_str_equals("packed representation", expected, buf);

	test("pack: non-special string encoding");
	original = TEST_PACK_SYRUS;
	expected = "\"" TEST_PACK_SYRUS "\"";
	nbytes = pack(buf, sizeof(buf), "a", original);
	assert_int_equals("pack should return bytes written", strlen(expected)+1, nbytes);
	assert_str_equals("packed representation", expected, buf);

	test("pack: string with one quote");
	original = "Example with a \" in it";
	expected = "\"Example with a \\\" in it\"";
	nbytes = pack(buf, sizeof(buf), "a", original);
	assert_int_equals("pack should return bytes written", strlen(expected)+1, nbytes);
	assert_str_equals("packed representation", expected, buf);

	test("pack: string with quotes and other escaped characters");
	original = "\"test\"\\t\\tDouble-tab \"plus quotes\"";
	expected = "\"\\\"test\\\"\\t\\tDouble-tab \\\"plus quotes\\\"\"";
	nbytes = pack(buf, sizeof(buf), "a", original);
	assert_int_equals("pack should return bytes written", strlen(expected)+1, nbytes);
	assert_str_equals("packed representation", expected, buf);
}

void test_pack_multiple_strings()
{
	char buf[256];
	size_t nbytes;
	char *str1, *str2, *str3;

	test("pack: Multiple, consecutive string encoding");
	nbytes = pack(buf, sizeof(buf), "aaa", "username", "passwd", "/home/directory");
	assert_int_equals("pack should return bytes written", 36, nbytes);
	assert_str_equals("packed representation", "\"username\"\"passwd\"\"/home/directory\"", buf);

	test("pack: Multple, consecutive string decoding");
	assert_int_equals("unpack should return 0", 0, unpack(buf, "aaa", &str1, &str2, &str3));
	assert_str_equals("string 1 should be 'username'", "username", str1);
	assert_str_equals("string 2 should be 'passwd'", "passwd", str2);
	assert_str_equals("string 3 should be '/home/directory'", "/home/directory", str3);

	free(str1);
	free(str2);
	free(str3);
}

void test_pack_decoding_integers()
{
	uint8_t  u8; uint16_t u16; uint32_t u32;
	 int8_t  i8;  int16_t i16;  int32_t i32;

	test("pack: unpack signed 8-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("d6", "c", &i8));
	assert_signed_equals("d6 should unpack to signed 8-bit integer -42", -42, i8);

	test("pack: unpack unsigned 8-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("2a", "C", &u8));
	assert_unsigned_equals("2a should unpack to unsigned 8-bit integer 42", 42, u8);

	test("pack: unpack signed 16-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("beef", "s", &i16));
	assert_signed_equals("beef should unpack to signed 16-bit integer -16657", -16657, i16);

	test("pack: unpack unsigned 16-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("beef", "S", &u16));
	assert_unsigned_equals("beef should unpack to unsigned 16-bit integer 48879", 48879, u16);

	test("pack: unpack signed 32-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("deadbeef", "l", &i32));
	// 1101 1110 1010 1101 1011 1110 1110 1111
	assert_signed_equals("deadbeef should unpack to signed 32-bit integer -559038737", -559038737, i32);

	test("pack: unpack unsigned 32-bit integer");
	assert_int_equals("unpack should return 0", 0, unpack("deadbeef", "L", &u32));
	assert_unsigned_equals("deadbeef should unpack to unsigned 32-bit integer 3735928559", 0xdeadbeef, u32);
}

void test_pack_decoding_strings()
{
	char *buf = NULL;
	const char *packed, *expected;

	test("pack: NULL/empty string decoding");
	packed   = "\"\"";
	expected = "";
	assert_int_equals("unpack should return 0", 0, unpack(packed, "a", &buf));
	assert_str_equals("unpacked string", expected, buf);
	free(buf); buf = NULL;

	test("pack: non-special string encoding");
	packed   = "\"" TEST_PACK_SYRUS "\"";
	expected = TEST_PACK_SYRUS;
	assert_int_equals("unpack should return 0", 0, unpack(packed, "a", &buf));
	assert_str_equals("unpacked string", expected, buf);
	free(buf); buf = NULL;

	test("pack: string with one quote");
	expected = "Example with a \" in it";
	packed   = "\"Example with a \\\" in it\"";
	assert_int_equals("unpack should return 0", 0, unpack(packed, "a", &buf));
	assert_str_equals("unpacked string", expected, buf);
	free(buf); buf = NULL;

	test("pack: string with quotes and other escaped characters");
	packed   = "\"\\\"test\\\"\\t\\tDouble-tab \\\"plus quotes\\\"\"";
	expected = "\"test\"\\t\\tDouble-tab \"plus quotes\"";
	assert_int_equals("unpack should return 0", 0, unpack(packed, "a", &buf));
	assert_str_equals("unpacked string", expected, buf);
	free(buf); buf = NULL;
}

void test_pack_size_querying()
{
	test("pack: Querying for buffer size (NULL buffer test)");
	assert_int_equals("pack('c') needs 2+1 bytes", 3, pack(NULL, 0, "c", 42));
	assert_int_equals("pack('C') needs 2+1 bytes", 3, pack(NULL, 0, "C", 42));
	assert_int_equals("pack('s') needs 4+1 bytes", 5, pack(NULL, 0, "s", 42));
	assert_int_equals("pack('S') needs 4+1 bytes", 5, pack(NULL, 0, "S", 42));
	assert_int_equals("pack('l') needs 8+1 bytes", 9, pack(NULL, 0, "l", 42));
	assert_int_equals("pack('L') needs 8+1 bytes", 9, pack(NULL, 0, "L", 42));

	/* more complicated tests */
	assert_int_equals("pack('cCsSlL') needs 28+1 bytes", 29, pack(NULL, 0, "cCsSlL", 42, 42, 420, 420, 42000, 42000));
	assert_int_equals("pack('') needs 1 byte", 1, pack(NULL, 0, ""));

	test("pack: query-mode string deref segfault bug");
	/* the quotes add two bytes per string, thus the extra 4 bytes */
	assert_int_equals("pack('sala') needs 24+1 bytes", 25, pack(NULL, 0, "sala", 42, "aabb", 42, "bbaa"));
}

void test_pack_DECAFBAD()
{
	size_t nbytes;
	char buf[16];

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
	nbytes = pack(buf, sizeof(buf), "CCCC", 222, 202, 251, 173);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of 222 202 251 173", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD as 2 unsigned 16-bit integers");
	nbytes = pack(buf, sizeof(buf), "SS", 57034, 64429);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of 57034 64429", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD as 1 unsigned 32-bit integers");
	nbytes = pack(buf, sizeof(buf), "L", 3737844653UL);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of 3737844653", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD as 2 unsigned 8-bit and 1 unsigned 16-bit integers");
	nbytes = pack(buf, sizeof(buf), "CCS", 222, 202, 64429);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of 222 202 64429", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD as 1 unsigned 16-bit and 2 unsigned 8-bit integers");
	nbytes = pack(buf, sizeof(buf), "SCC", 57034, 251, 173);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of 57034 251 173", "decafbad", buf);

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
	nbytes = pack(buf, sizeof(buf), "cccc", -34, -54, -5, -83);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of -34 -54 -5 -83", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD as 2 signed 16-bit integers");
	nbytes = pack(buf, sizeof(buf), "ss", -8502, -1107);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of -8502 -1107", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD as 1 signed 32-bit integers");
	nbytes = pack(buf, sizeof(buf), "l", 0xDECAFBAD);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of 3737844653", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD as 2 signed 8-bit and 1 signed 16-bit integers");
	nbytes = pack(buf, sizeof(buf), "ccs", -34, -54, -1107);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of -34 -54 -1107", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD as 1 signed 16-bit and 2 signed 8-bit integers");
	nbytes = pack(buf, sizeof(buf), "scc", -8502, -5, -83);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of -8502 -5 -83", "decafbad", buf);

	/* Mixing of signed and unsigned types works as well.
	   Here are a few, for illustration.

	   -34 202 -5 173
	   -8502 251 173

	   The remainder of the tests exercise mixed sign packing
	 */
	test("pack: Packing 0xDECAFBAD by alternating signed 8-bit and unsigned 8-bit");
	nbytes = pack(buf, sizeof(buf), "cCcC", -34, 202, -5, 173);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of -34 202, -5, 173", "decafbad", buf);

	test("pack: Packing 0xDECAFBAD by with 1 signed 16-bit and 2 unsigned 8-bit integers");
	nbytes = pack(buf, sizeof(buf), "sCC", -8502, 251, 173);
	assert_int_equals("pack should return 9 bytes written", 9, nbytes);
	assert_str_equals("packed representation of -8502 251 173", "decafbad", buf);
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
		unpack("decafbad", "CCCC", &u8a, &u8b, &u8c, &u8d));
	assert_unsigned_equals("u8a should be 222", 222, u8a);
	assert_unsigned_equals("u8b should be 202", 202, u8b);
	assert_unsigned_equals("u8c should be 251", 251, u8c);
	assert_unsigned_equals("u8d should be 173", 173, u8d);

	u8a = u8b = u8c = u8d = u16a = u16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 2 unsigned 16-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "SS", &u16a, &u16b));
	assert_unsigned_equals("u16a should be 57034", 57034, u16a);
	assert_unsigned_equals("u16b should be 64429", 64429, u16b);

	u8a = u8b = u8c = u8d = u16a = u16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 2 unsigned 8-bit and 1 unsigned 16-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "CCS", &u8a, &u8b, &u16b));
	assert_unsigned_equals("u8a should be 222", 222, u8a);
	assert_unsigned_equals("u8b should be 202", 202, u8b);
	assert_unsigned_equals("u16b should be 64429", 64429, u16b);

	u8a = u8b = u8c = u8d = u16a = u16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 1 unsigned 16-bit and 2 unsigned 8-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "SCC", &u16a, &u8a, &u8b));
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
		unpack("decafbad", "CCCC", &i8a, &i8b, &i8c, &i8d));
	assert_signed_equals("i8a should be -34", -34, i8a);
	assert_signed_equals("i8b should be -54", -54, i8b);
	assert_signed_equals("i8c should be -5", -5, i8c);
	assert_signed_equals("i8d should be -83", -83, i8d);

	i8a = i8b = i8c = i8d = i16a = i16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 2 signed 16-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "SS", &i16a, &i16b));
	assert_signed_equals("i16a should be -8502", -8502, i16a);
	assert_signed_equals("i16b should be -1107", -1107, i16b);

	i8a = i8b = i8c = i8d = i16a = i16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 2 signed 8-bit and 1 signed 16-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "CCS", &i8a, &i8b, &i16b));
	assert_signed_equals("i8a should be -34", -34, i8a);
	assert_signed_equals("i8b should be -54", -54, i8b);
	assert_signed_equals("i16b should be -1107", -1107, i16b);

	i8a = i8b = i8c = i8d = i16a = i16b = 0;
	test("pack: Unpacking 0xDECAFBAD as 1 signed 16-bit and 2 signed 8-bit integers");
	assert_int_equals("unpack should return 0", 0,
		unpack("decafbad", "SCC", &i16a, &i8a, &i8b));
	assert_signed_equals("i16b should be -8502", -8502, i16a);
	assert_signed_equals("i8a should be -5", -5, i8a);
	assert_signed_equals("i8b should be -83", -83, i8b);


	/* Mixing of signed and unsigned types works as well.
	   Here are a few, for illustration.

	     -34 202 -5 173
	     -8502 251 173

	 */


}

void test_suite_pack() {
	test_pack_encoding_integers();
	test_pack_encoding_strings();
	test_pack_multiple_strings();

	test_pack_decoding_integers();
	test_pack_decoding_strings();

	test_pack_size_querying();
	test_pack_DECAFBAD();
	test_pack_interpretation();
}
