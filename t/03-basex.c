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

TESTS {
	subtest {
		char     ascii[256];
		uint8_t  binary[128];

		memcpy(ascii, "decafbad", 8);
		int rc = base16_decode(binary, 128, ascii, 8);
		is_int(rc, 4, "4 bytes used to decode 'decafbad'");
		is_int(binary[0], 0xde, "decafbad: Byte #1 decoded");
		is_int(binary[1], 0xca, "decafbad: Byte #2 decoded");
		is_int(binary[2], 0xfb, "decafbad: Byte #3 decoded");
		is_int(binary[3], 0xad, "decafbad: Byte #4 decoded");

		memcpy(binary, "\xde\xad\xbe\xef", 4);
		rc = base16_encode(ascii, 256, binary, 4);
		is_int(rc, 8, "8 bytes to encode 'deadbeef'");
		ascii[rc] = '\0';
		is_string(ascii, "deadbeef", "Encoded binary into base16 ASCII");
	}

	subtest { /* the empty string */
		char buf[16] = "untouched";
		int rc = base16_encode(buf, 16, "", 0);
		is_int(rc, 0, "encode('') == '' (no bytes)");
		is_string(buf, "untouched", "destination buffer left as-is on encode");

		rc = base16_decode(buf, 16, "", 0);
		is_int(rc, 0, "decode('') == '' (no bytes)");
		is_string(buf, "untouched", "destination buffer left as-is on decode");
	}

	subtest { /* ASCII -> ASCII (longer strings) */
		char encoded[4096];
		char decoded[2048];

		const char *quote = "Our wasted oil unprofitably burns, Like hidden lamps in old sepulchral urns.";
		                     /* _Conversation_, William Cowper */

		int rc = base16_encode(encoded, 4096, quote, strlen(quote));
		is_int(rc, strlen(quote) * 2, "Twice as many bytes needed for base16 encoding");
		encoded[rc] = '\0';
		is_string(encoded, "4f757220776173746564206f696c20756e70726f66697461626c79206275726e"
		                   "732c204c696b652068696464656e206c616d707320696e206f6c642073657075"
		                   "6c636872616c2075726e732e",
			"Encoded ASCII values");

		rc = base16_decode(decoded, 2048, encoded, strlen(encoded));

		is_int(rc, strlen(quote), "Half as many bytes needed for base16 decoding");
		decoded[rc] = '\0';
		is_string(decoded, quote, "Got back the original quote on encode -> decode");
	}

	subtest { /* convenience methods */
		char *encoded, *decoded;
		const char *quote = "For courage mounteth with occasion.";
		                    /* _King John_, act ii, William Shakespeare */

		encoded = base16_encodestr(quote, strlen(quote));
		isnt_null(encoded, "base16_encodestr() returns a non-NULL pointer");
		is_string(encoded, "466f7220636f7572616765206d6f756e7"
		                   "46574682077697468206f63636173696f"
		                   "6e2e",
			"Encoded string properly");

		decoded = base16_decodestr(encoded, strlen(encoded));
		isnt_null(decoded, "base16_decodestr() returns a non-NULL pointer");
		is_string(decoded, quote, "Decoded properly");

		free(encoded);
		free(decoded);
	}

	subtest { /* case conversion */
		char *encoded, *decoded;
		const char *quote = "What is love?";
		                    /* - Haddaway */

		encoded = base16_encodestr(quote, strlen(quote));
		is_string(encoded, "57686174206973206c6f76653f", "Encoded using lower-case a-f");

		decoded = base16_decodestr("57686174206973206C6F76653F", strlen(encoded));
		is_string(decoded, quote, "Decode can handle upper-case a-f");

		free(encoded);
		free(decoded);
	}

	subtest { /* invalid calls */
		char buf[8192];

		errno = 0;
		cmp_ok(base16_encode(buf, 1, "decafbad", 8), "<", 0,
			"Failed to encode with short-stroked destination buffer");
		is_int(errno, EINVAL, "errno set to EINVAL when dst buffer is too small");

		errno = 0;
		cmp_ok(base16_decode(buf, 1, "decafbad", 8), "<", 0,
			"Failed to decode with short-stroked destination buffer");
		is_int(errno, EINVAL, "errno set to EINVAL when dst buffer is too small");

		errno = 0;
		cmp_ok(base16_decode(buf, 8191, "INVALID", 7), "<", 0,
			"Failed to decode with invalid hex characters");
		is_int(errno, EILSEQ, "errno set to EILSEQ on invalid encoded character");
	}

	done_testing();
}
