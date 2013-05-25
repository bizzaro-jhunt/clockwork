/*
  Copyright 2011-2013 James Hunt <james@jameshunt.us>

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
#include "../src/gear/gear.h"

/* "There's no point in seeking a remedy for a thunderbolt" - Syrus, _Maxims_ */
#define SYRUS "Remedium frustra est contra fulmen quaerere"

int main(void) {
	test();

	subtest {
		uint8_t  u8; uint16_t u16; uint32_t u32;
		 int8_t  i8;  int16_t i16;  int32_t i32;
		char *p;

		/* -42 is binary 1101 0110, decimal 214 and hex d6 */
		is_string(p = pack("", "c", -42), "d6", "packed -42");
		ok(unpack(p, "", "c", &i8) == 0, "unpacked int8");
		is_signed(i8, -42, "unpacked value for d6/c");
		free(p);

		is_string(p = pack("", "C", 125), "7d", "packed 125");
		ok(unpack(p, "", "C", &u8) == 0, "unpacked uint8");
		is_unsigned(u8, 125, "unpacked value for 7d/C");
		free(p);

		/* -21456 is 1010 110 0011 0000, decimal 44080, hex ac30 */
		is_string(p = pack("", "s", -21456), "ac30", "packed -21456");
		ok(unpack(p, "", "s", &i16) == 0, "unpacked int16");
		is_signed(i16, -21456, "unpacked value for ac30/s");
		free(p);

		is_string(p = pack("", "S", 1984), "07c0", "packed 1984");
		ok(unpack(p, "", "S", &u16) == 0, "unpacked uint16");
		is_unsigned(u16, 1984, "unpacked value for 07c0/S");
		free(p);

		/* -787856564 is binary 1101 0001 0000 1010 0100 001 0100 1100,
		                 decimal 3507110732, hex d10a434c */
		is_string(p = pack("", "l", -787856564), "d10a434c", "packed -787856563");
		ok(unpack(p, "", "l", &i32) == 0, "unpacked int32");
		is_int(i32, -787856564, "unpacked value for d10a434c/l");
		free(p);

		/* Jenny, I've got your number... */
		is_string(p = pack("", "L", 8675309), "00845fed", "packed 867-5309");
		ok(unpack(p, "", "L", &u32) == 0, "unpacked uin32");
		is_int(u32, 8675309, "unpacked value for 00845fed/L");
		free(p);
	}

	subtest {
		char *p;
		char *val;

		is_string(p = pack("", "a", NULL), "\"\"", "packed NULL string");
		ok(unpack(p, "", "a", &val) == 0, "unpacked NULL string");
		is_string(val, "", "unpacked NULL string => ''");
		free(val);
		free(p);

		is_string(p = pack("", "a", ""), "\"\"", "packed empty string");
		ok(unpack(p, "", "a", &val) == 0, "unpacked empty string");
		is_string(val, "", "unpacked empty string => ''");
		free(val);
		free(p);

		is_string(p = pack("", "a", SYRUS), "\"" SYRUS "\"",
			"packed a normal string");
		ok(unpack(p, "", "a", &val) == 0, "unpacked regular string");
		is_string(val, SYRUS, "unpacked regular string");
		free(val);
		free(p);

		is_string(p = pack("", "a", "Example with a \" in it"),
			"\"Example with a \\\" in it\"",
			"packed a string with a single double-quote");
		ok(unpack(p, "", "a", &val) == 0, "unpacked escaped-char string");
		is_string(val, "Example with a \" in it",
			"unpacked string with a single double-quote");
		free(val);
		free(p);

		is_string(p = pack("", "a", "\"test\"\\t\\tdouble-tab \"plus quotes\""),
			"\"\\\"test\\\"\\t\\tdouble-tab \\\"plus quotes\\\"\"",
			"packed a string with lots of escape chars");
		ok(unpack(p, "", "a", &val) == 0, "unpacked crazy string");
		is_string(val, "\"test\"\\t\\tdouble-tab \"plus quotes\"",
			"unpacked string with lots of escape chars");
		free(val);
		free(p);
	}

	subtest {
		char *p;
		char *s1, *s2, *s3;

		is_string(p = pack("", "aaa", "username", "passwd", "/home/directory"),
			"\"username\"\"passwd\"\"/home/directory\"",
			"packed multiple strings in series");

		ok(unpack(p, "", "aaa", &s1, &s2, &s3) == 0, "unpacked 'aaa' data");
		is_string(s1, "username",        "string #1");
		is_string(s2, "passwd",          "string #2");
		is_string(s3, "/home/directory", "string #3");

		free(p);
		free(s1);
		free(s2);
		free(s3);
	}

	subtest {
		char *s1, *s2, *s3;
		ok(unpack(
				"\"s1\"\"s2\"\"s3\"",
				"prefix::", "aaa",
				&s1,  &s2,  &s3
			) != 0, "unpack fails with bad prefix");

		ok(unpack(
				"<invalid data>",
				"prefix::", "aaa",
				&s1, &s2, &s3
			) != 0, "unpack fails with bad data");
	}

	subtest {
		char *s1, *s2, *s3;
		char *packed;

		isnt_null(
			packed = pack("bad::", "aaZ", "string1", "string2", "string3"),
			"pack() handles bad format strings");
		is_string(
			packed, "bad::\"string1\"\"string2\"",
			"pack() ignores bad format options");
		free(packed);

		s1 = s2 = s3 = NULL;
		ok(unpack("bad::\"string1\"\"string2\"\"string3\"",
			"bad::", "aaZ", &s1, &s2, &s3) == 0,
			"unpack() handles bad format strings");
		is_string(s1, "string1", "s1 set properly ('a' format)");
		is_string(s2, "string2", "s2 set properly ('a' format)");
		is_null(s3, "s3 was ignored (invalid 'Z' format)");

		free(s1);
		free(s2);
		free(s3);
	}

	subtest {
		uint8_t u8a, u8b, u8c, u8d;
		 int8_t i8a, i8b, i8c, i8d;

		uint16_t u16a, u16b;
		 int16_t i16a, i16b;

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
		is_string(p = pack("", "CCCC", 222, 202, 251, 173), "decafbad",
			"packed 0xDECAFBAD as CCCC");
		u8a = u8b = u8c = u8d = u16a = u16b = 0;
		ok(unpack("decafbad", "", "CCCC", &u8a, &u8b, &u8c, &u8d) == 0,
			"unpacked 0xDECAFBAD as CCCC");
		is_unsigned(u8a, 222, "unpacked decafbad u8[0]");
		is_unsigned(u8b, 202, "unpacked decafbad u8[1]");
		is_unsigned(u8c, 251, "unpacked decafbad u8[2]");
		is_unsigned(u8d, 173, "unpacked decafbad u8[3]");
		free(p);

		is_string(p = pack("", "SS", 57034, 64429), "decafbad",
			"packed 0xDECAFBAD as SS");
		u8a = u8b = u8c = u8d = u16a = u16b = 0;
		ok(unpack("decafbad", "", "SS", &u16a, &u16b) == 0,
			"unpacked 0xDECAFBAD as SS");
		is_unsigned(u16a, 57034, "unpacked decafbad u16[0]");
		is_unsigned(u16b, 64429, "unpacked decafbad u16[1]");
		free(p);

		is_string(p = pack("", "L", 3737844653UL), "decafbad",
			"packed 0xDECAFBAD as 1 unsigned 32-bit integer (L)");
		free(p);

		is_string(p = pack("", "CCS", 222, 202, 64429), "decafbad",
			"packed 0xDECAFBAD as CCS");
		u8a = u8b = u8c = u8d = u16a = u16b = 0;
		ok(unpack("decafbad", "", "CCS", &u8a, &u8b, &u16b) == 0,
			"unpacked 0xDECAFBAD as CCS");
		is_unsigned(u8a, 222, "unpacked decafbad u8[0]");
		is_unsigned(u8b, 202, "unpacked decafbad u8[1]");
		is_unsigned(u16b, 64429, "unpacked decafbad u16[1]");
		free(p);

		is_string(p = pack("", "SCC", 57034, 251, 173), "decafbad",
			"packed 0XDECAFBAD as SCC");
		u8a = u8b = u8c = u8d = u16a = u16b = 0;
		ok(unpack("decafbad", "", "SCC", &u16a, &u8c, &u8d) == 0,
			"unpacked 0xDECAFBAD as SCC");
		is_unsigned(u16a, 57034, "unpacked decafbad u16[0]");
		is_unsigned(u8c, 251, "unpacked decafbad u8[2]");
		is_unsigned(u8d, 173, "unpacked decafbad u8[3]");
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
		is_string(p = pack("", "cccc", -34, -54, -5, -83), "decafbad",
			"packed 0xDECAFBAD as cccc");
		i8a = i8b = i8c = i8d = i16a = i16b = 0;
		ok(unpack("decafbad", "", "CCCC", &i8a, &i8b, &i8c, &i8d) == 0,
			"unpacked 0xDECAFBAD as CCCC");
		is_signed(i8a, -34, "unpacked DECAFBAD i8[0]");
		is_signed(i8b, -54, "unpacked DECAFBAD i8[1]");
		is_signed(i8c,  -5, "unpacked DECAFBAD i8[2]");
		is_signed(i8d, -83, "unpacked DECAFBAD i8[3]");
		free(p);

		is_string(p = pack("", "ss", -8502, -1107), "decafbad",
			"packed 0xDECAFBAD as ss");
		i8a = i8b = i8c = i8d = i16a = i16b = 0;
		ok(unpack("decafbad", "", "SS", &i16a, &i16b) == 0,
			"unpacked 0xDECAFBAD as SS");
		is_signed(i16a, -8502, "unpacked DECAFBAD i16[0]");
		is_signed(i16b, -1107, "unpacked DECAFBAD i16[1]");
		free(p);

		is_string(p = pack("", "l", 0xDECAFBAD), "decafbad",
			"packed 0xDECAFBAD as 1 signed 32-bit integer (l)");
		free(p);

		is_string(p = pack("", "CCS", -34, -54, -1107), "decafbad",
			"packed 0xDECAFBAD as CCS");
		i8a = i8b = i8c = i8d = i16a = i16b = 0;
		ok(unpack("decafbad", "", "CCS", &i8a, &i8b, &i16b) == 0,
			"unpacked 0xDECAFBAD as CCS");
		is_signed(i8a,    -34, "unpacked DECAFBAD i8[0]");
		is_signed(i8b,    -54, "unpacked DECAFBAD i8[1]");
		is_signed(i16b, -1107, "unpacked DECAFBAD i16[1]");
		free(p);

		is_string(p = pack("", "scc", -8502, -5, -83), "decafbad",
			"packed 0xDECAFBAD as scc");
		i8a = i8b = i8c = i8d = i16a = i16b = 0;
		ok(unpack("decafbad", "", "SCC", &i16a, &i8c, &i8d) == 0,
			"unpacked 0xDECAFBAD as SCC");
		is_signed(i16a, -8502, "unpacked DECAFBAD i16[0]");
		is_signed(i8c,     -5, "unpacked DECAFBAD i8[2]");
		is_signed(i8d,    -83, "unpacked DECAFBAD i8[3]");
		free(p);

		/* Mixing of signed and unsigned types works as well.
		   Here are a few, for illustration.

		   -34 202 -5 173
		   -8502 251 173

		   The remainder of the tests exercise mixed sign packing
		 */
		is_string(p = pack("", "cCcC", -34, 202, -5, 173), "decafbad",
			"packed 0xDECAFBAD as cCcC");
		free(p);

		is_string(p = pack("", "sCC", -8502, 251, 173), "decafbad",
			"packed 0xDECAFBAD as sCC");
		free(p);
	}

	subtest {
		unsigned long long unpacked = 0;
		unsigned long long ull = 0xdecafbad00;
		char *packed;

		isnt_null(packed = pack("overflow::", "L", ull),
			"packed still succeeds with 64-bit values");

		is_string(packed, "overflow::cafbad00", "pack truncated");
		ok(unpack(packed, "overflow::", "L", &unpacked) == 0,
			"unpacked truncated value");
		is_unsigned(unpacked, 0xcafbad00, "got truncated value");
	}

	done_testing();
}
