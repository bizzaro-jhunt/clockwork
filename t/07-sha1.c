/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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
#include "../src/sha1.h"

#define FIPS1_IN "abc"
#define FIPS2_IN "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"

#define FIPS1_OUT "a9993e36" "4706816a" "ba3e2571" "7850c26c" "9cd0d89d"
#define FIPS2_OUT "84983e44" "1c3bd26e" "baae4aa1" "f95129e5" "e54670f1"
#define FIPS3_OUT "34aa973c" "d4c4daa4" "f61eeb2b" "dbad2731" "6534016f"

TESTS {
	struct SHA1 cksum, calc, init;
	struct sha1_ctx ctx;
	int i;

	sha1_init(&cksum, NULL);
	sha1_data(FIPS1_IN, strlen(FIPS1_IN), &cksum);
	is_string(cksum.hex, FIPS1_OUT, "FIPS Pub 180-1 test vector #1");

	sha1_init(&cksum, NULL);
	sha1_data(FIPS2_IN, strlen(FIPS2_IN), &cksum);
	is_string(cksum.hex, FIPS2_OUT, "FIPS Pub 180-1 test vector #2");

	/* it's hard to define a string constant for 1 mil 'a's... */
	sha1_ctx_init(&ctx);
	for (i = 0; i < 1000000; ++i) {
		sha1_ctx_update(&ctx, (unsigned char *)"a", 1);
	}
	sha1_ctx_final(&ctx, cksum.raw);
	sha1_hexdigest(&cksum);
	is_string(cksum.hex, FIPS3_OUT, "FIPS Pub 180-1 test vector #3");


	sha1_init(&calc, NULL);
	is_string(calc.hex, "", "initial blank checksum");

	/* Borrow from FIPS checks */
	sha1_data("this is a test", strlen("this is a test"), &calc);
	sha1_init(&init, calc.hex);
	is_string(calc.hex, init.hex, "init.hex == calc.hex");

	ok(memcmp(init.raw, calc.raw, SHA1_DIGLEN) == 0, "raw digests are equiv.");

	sha1_init(&calc, "0123456789abcd0123456789abcd0123456789ab");
	isnt_string(calc.hex, "", "valid checksum is not empty string");

	sha1_init(&calc, "0123456789abcd0123456789");
	is_string(calc.hex, "", "invalid initial checksum (too short)");

	sha1_init(&calc, "0123456789abcd0123456789abcdefghijklmnop");
	is_string(calc.hex, "", "invalid initial checksum (non-hex digits)");


	struct SHA1 a, b;
	const char *s1 = "This is the FIRST string";
	const char *s2 = "This is the SECOND string";

	sha1_init(&a, NULL);
	sha1_init(&b, NULL);

	sha1_data(s1, strlen(s1), &a);
	sha1_data(s2, strlen(s2), &b);

	ok(sha1_cmp(&a, &a) == 0, "identical checksums are equal");
	ok(sha1_cmp(&a, &b) != 0, "different checksums are not equal");


	ok(sha1_file("/tmp",  &cksum) < 0, "sha1_file on directory fails");
	ok(sha1_file("/nope", &cksum) < 0, "sha1_file on non-existent file fails");


	done_testing();
}
