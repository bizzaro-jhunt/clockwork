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

#define EMPTY_BIN_KEY "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
#define EMPTY_B16_KEY "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"

TESTS {
	subtest { /* basic key generation */
		cw_cert_t *key = cw_cert_new();
		ok(!key->pubkey, "newly-created key has no public key");
		ok(!key->seckey, "newly-created key has no secret key");
		ok(memcmp(key->pubkey_b16, EMPTY_B16_KEY, 64) == 0,
			"public key (base16) is all zeros on creation");
		ok(memcmp(key->pubkey_bin, EMPTY_BIN_KEY, 32) == 0,
			"public key (binary) is all zeros on creation");
		ok(memcmp(key->seckey_b16, EMPTY_B16_KEY, 64) == 0,
			"secret key (base16) is all zeros on creation");
		ok(memcmp(key->seckey_bin, EMPTY_BIN_KEY, 32) == 0,
			"secret key (binary) is all zeros on creation");

		cw_cert_destroy(key);
		key = cw_cert_generate();
		isnt_null(key, "cw_cert_generate() returned a pointer");

		ok(key->pubkey, "newly-generated key has a public key");
		ok(key->seckey, "newly-generated key has a secret key");
		ok(memcmp(key->pubkey_b16, EMPTY_B16_KEY, 64) != 0,
			"public key (base16) is not all zeros");
		ok(memcmp(key->pubkey_bin, EMPTY_BIN_KEY, 32) != 0,
			"public key (binary) is not all zeros");
		ok(memcmp(key->seckey_b16, EMPTY_B16_KEY, 64) != 0,
			"secret key (base16) is not all zeros");
		ok(memcmp(key->seckey_bin, EMPTY_BIN_KEY, 32) != 0,
			"secret key (binary) is not all zeros");

		cw_cert_t *other = cw_cert_generate();
		ok(memcmp(key->pubkey_bin, other->pubkey_bin, 32) != 0,
			"cw_cert_generate() generates unique public keys");
		ok(memcmp(key->seckey_bin, other->pubkey_bin, 32) != 0,
			"cw_cert_generate() generates unique secret keys");

		cw_cert_destroy(key);
		cw_cert_destroy(other);
	}

	subtest { /* read in (combined pub+sec) */
		cw_cert_t *key;

		key = cw_cert_read("/dev/null");
		is_null(key, "Can't read a key from /dev/null");

		key = cw_cert_read("/path/to/enoent");
		is_null(key, "Can't read a key from a nonexistent file");

		key = cw_cert_read(TEST_DATA "/certs/combined.crt");
		isnt_null(key, "Read combined certificate from certs/combined.crt");
		ok(key->pubkey, "combined.crt contains a public key");
		ok(key->seckey, "combined.crt contains a secret key");

		is_string(key->pubkey_b16, "0394ba6c5dda02a38ad80ff80560179e8c52a9cb178d18d86490d04149ab3546",
			"Read the correct public key from combined.crt");
		ok(memcmp(key->pubkey_bin, "\x03\x94\xba\x6c\x5d\xda\x02\xa3\x8a\xd8\x0f\xf8\x05\x60\x17\x9e"
		                           "\x8c\x52\xa9\xcb\x17\x8d\x18\xd8\x64\x90\xd0\x41\x49\xab\x35\x46", 32) == 0,
			"Read the correct public key (binary) from combined.crt");

		char *s;
		is_string(s = cw_cert_public_s(key),
			"0394ba6c5dda02a38ad80ff80560179e8c52a9cb178d18d86490d04149ab3546",
			"Read the correct public key from combined.crt (via cw_cert_public_s)");
		free(s);
		ok(memcmp(cw_cert_public(key),
			"\x03\x94\xba\x6c\x5d\xda\x02\xa3\x8a\xd8\x0f\xf8\x05\x60\x17\x9e"
			"\x8c\x52\xa9\xcb\x17\x8d\x18\xd8\x64\x90\xd0\x41\x49\xab\x35\x46", 32) == 0,
			"Read the correct public key (binary) from combined.crt (via cw_curvkey_public)");

		is_string(key->seckey_b16, "feb546ed444dddb23c9a3c7a72236cf32c90148649d4563d8264665f248db516",
			"Read the correct secret key from combined.crt");
		ok(memcmp(key->seckey_bin, "\xfe\xb5\x46\xed\x44\x4d\xdd\xb2\x3c\x9a\x3c\x7a\x72\x23\x6c\xf3"
		                           "\x2c\x90\x14\x86\x49\xd4\x56\x3d\x82\x64\x66\x5f\x24\x8d\xb5\x16", 32) == 0,
			"Read the correct secret key (binary) from combined.crt");

		is_string(s = cw_cert_secret_s(key),
			"feb546ed444dddb23c9a3c7a72236cf32c90148649d4563d8264665f248db516",
			"Read the correct secret key from combined.crt (via cw_cert_secret_s)");
		free(s);
		ok(memcmp(cw_cert_secret(key),
			"\xfe\xb5\x46\xed\x44\x4d\xdd\xb2\x3c\x9a\x3c\x7a\x72\x23\x6c\xf3"
			"\x2c\x90\x14\x86\x49\xd4\x56\x3d\x82\x64\x66\x5f\x24\x8d\xb5\x16", 32) == 0,
			"Read the correct secret key (binary) from combined.crt (via cw_cert_secret)");

		is_string(key->ident, "some.host.some.where", "Read identity from combined.crt");

		cw_cert_destroy(key);
	}

	subtest { /* read in (pub) */
		cw_cert_t *key = cw_cert_read(TEST_DATA "/certs/public.crt");
		isnt_null(key, "Read public certificate from certs/public.crt");
		ok( key->pubkey, "public.crt contains a public key");
		ok(!key->seckey, "public.crt does not contain a secret key");

		is_string(key->pubkey_b16, "0394ba6c5dda02a38ad80ff80560179e8c52a9cb178d18d86490d04149ab3546",
			"Read the correct public key from public.crt");
		ok(memcmp(key->pubkey_bin, "\x03\x94\xba\x6c\x5d\xda\x02\xa3\x8a\xd8\x0f\xf8\x05\x60\x17\x9e"
		                           "\x8c\x52\xa9\xcb\x17\x8d\x18\xd8\x64\x90\xd0\x41\x49\xab\x35\x46", 32) == 0,
			"Read the correct public key (binary) from public.crt");
		is_string(key->ident, "some.host.some.where", "Read identity from public.crt");

		cw_cert_destroy(key);
	}

	subtest { /* write out (full) */
		cw_cert_t *key = cw_cert_generate();
		key->ident = strdup("a.new.host.to.test");

		isnt_int(cw_cert_write(key, "/root/permdenied", 1), 0,
			"cw_cert_write() fails on EPERM");
		isnt_int(cw_cert_write(key, "/path/to/enoent", 1), 0,
			"cw_cert_write() fails on bad parent path");

		is_int(cw_cert_write(key, TEST_TMP "/newfull.crt", 1), 0,
			"wrote certificate (pub+sec) to newfull.crt");

		cw_cert_t *other = cw_cert_read(TEST_TMP "/newfull.crt");
		isnt_null(other, "read newly-written certificate (pub+sub)");
		is_string(other->pubkey_b16, key->pubkey_b16, "pubkey matches (write/reread)");
		is_string(other->seckey_b16, key->seckey_b16, "seckey matches (write/reread)");

		cw_cert_destroy(key);
		cw_cert_destroy(other);
	}

	subtest { /* write out (!full) */
		cw_cert_t *key = cw_cert_generate();
		key->ident = strdup("a.new.host.to.test");

		isnt_int(cw_cert_write(key, "/root/permdenied", 1), 0,
			"cw_cert_write() fails on EPERM");
		isnt_int(cw_cert_write(key, "/path/to/enoent", 1), 0,
			"cw_cert_write() fails on bad parent path");

		is_int(cw_cert_write(key, TEST_TMP "/newpub.crt", 0), 0,
			"wrote certificate (pub) to newpub.crt");

		cw_cert_t *other = cw_cert_read(TEST_TMP "/newpub.crt");
		isnt_null(other, "read newly-written certificate (pub)");
		is_string(other->pubkey_b16, key->pubkey_b16, "pubkey matches (write/reread)");
		isnt_string(other->seckey_b16, key->seckey_b16, "seckey does not match (write/reread)");

		cw_cert_destroy(key);
		cw_cert_destroy(other);
	}

	subtest { /* change keys * rescan */
		cw_cert_t *key = cw_cert_generate();
		uint8_t oldpub[32], oldsec[32];

		memcpy(oldpub, key->pubkey_bin, 32);
		memcpy(oldsec, key->seckey_bin, 32);

		memset(key->pubkey_b16, '8', 64);
		memset(key->seckey_b16, 'e', 64);

		ok(memcmp(oldpub, key->pubkey_bin, 32) == 0, "old public key still in force (pre-rescan)");
		ok(memcmp(oldsec, key->seckey_bin, 32) == 0, "old secret key still in force (pre-rescan)");

		is_int(cw_cert_rescan(key), 0, "recanned changed keypair");
		ok(memcmp(oldpub, key->pubkey_bin, 32) != 0, "old public key still no longer valid");
		ok(memcmp(oldsec, key->seckey_bin, 32) != 0, "old secret key still no longer valid");

		cw_cert_destroy(key);
	}

	/************************************************************************/

	subtest { /* CA basics */
		cw_trustdb_t *ca = cw_trustdb_new();
		isnt_null(ca, "cw_trustdb_new() returns a new CA pointer");
		ok(ca->verify, "By default, CA will verify certs");
		cw_trustdb_destroy(ca);
	}

	subtest { /* verify modes */
		cw_trustdb_t *ca = cw_trustdb_new();
		assert(ca);

		cw_cert_t *key = cw_cert_generate();
		assert(key);
		key->ident = strdup("original.host.name");

		ca->verify = 0;
		ok(cw_trustdb_verify(ca, key) == 0, "[verify=no] new key is verified (untrusted)");
		ca->verify = 1;
		ok(cw_trustdb_verify(ca, key) != 0, "[verify=yes] new key is not yet verified (untrusted)");

		is_int(cw_trustdb_trust(ca, key), 0, "trusted new key/cert");

		ca->verify = 0;
		ok(cw_trustdb_verify(ca, key) == 0, "[verify=no] new key is verified (trusted)");
		ca->verify = 1;
		ok(cw_trustdb_verify(ca, key) == 0, "[verify=yes] new key is verified (trusted)");

		is_int(cw_trustdb_revoke(ca, key), 0, "revoked key/cert");

		ca->verify = 0;
		ok(cw_trustdb_verify(ca, key) == 0, "[verify=no] new key is verified (revoked)");
		ca->verify = 1;
		ok(cw_trustdb_verify(ca, key) != 0, "[verify=yes] new key is not verified (revoked)");

		cw_cert_destroy(key);
		cw_trustdb_destroy(ca);
	}

	subtest { /* read CA list */
		cw_trustdb_t *ca = cw_trustdb_read(TEST_DATA "/certs/trusted");
		isnt_null(ca, "read certificate authority from file");

		cw_cert_t *other = cw_cert_generate();
		isnt_null(other, "generated new (untrusted) key");

		cw_cert_t *key = cw_cert_read(TEST_DATA "/certs/combined.crt");
		isnt_null(key, "read key from combined.crt");

		is_int(cw_trustdb_verify(ca, key), 0, "key for some.host.some.where is verified");
		isnt_int(cw_trustdb_verify(ca, other), 0, "new key not trusted");

		cw_trustdb_destroy(ca);
		cw_cert_destroy(other);
		cw_cert_destroy(key);
	}

	subtest { /* write CA list */
		cw_trustdb_t *ca = cw_trustdb_read(TEST_DATA "/certs/trusted");
		isnt_null(ca, "read certificate authority from file");

		cw_cert_t *other = cw_cert_generate();
		isnt_null(other, "generated new (untrusted) key");
		other->ident = strdup("xyz.host");

		cw_cert_t *key = cw_cert_read(TEST_DATA "/certs/combined.crt");
		isnt_null(key, "read key from combined.crt");

		cw_trustdb_revoke(ca, key);
		cw_trustdb_trust(ca, other);

		is_int(cw_trustdb_write(ca, TEST_TMP "/trusted"), 0,
			"wrote certificate authority to " TEST_TMP "/trusted");

		cw_trustdb_destroy(ca);
		ca = cw_trustdb_read(TEST_TMP "/trusted");
		isnt_null(ca, "re-read certificate authority");

		isnt_int(cw_trustdb_verify(ca, key), 0, "some.host.some.where not trusted by on-disk CA");
		is_int(cw_trustdb_verify(ca, other), 0, "xyz.host trusted by on-disk CA");

		cw_trustdb_destroy(ca);
		cw_cert_destroy(key);
		cw_cert_destroy(other);
	}

	done_testing();
}
