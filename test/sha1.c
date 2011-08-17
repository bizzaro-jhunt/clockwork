#include "test.h"
#include "assertions.h"
#include "../sha1.h"

#define FIPS1_IN "abc"
#define FIPS2_IN "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"

#define FIPS1_OUT "a9993e36" "4706816a" "ba3e2571" "7850c26c" "9cd0d89d"
#define FIPS2_OUT "84983e44" "1c3bd26e" "baae4aa1" "f95129e5" "e54670f1"
#define FIPS3_OUT "34aa973c" "d4c4daa4" "f61eeb2b" "dbad2731" "6534016f"

void test_sha1_FIPS()
{
	sha1 cksum;
	sha1_ctx ctx;
	int i;

	test("SHA1: FIPS Pub 180-1 test vectors");

	sha1_init(&cksum, NULL);
	sha1_data(FIPS1_IN, strlen(FIPS1_IN), &cksum);
	assert_str_eq("sha1(" FIPS1_IN ")", cksum.hex, FIPS1_OUT);

	sha1_init(&cksum, NULL);
	sha1_data(FIPS2_IN, strlen(FIPS2_IN), &cksum);
	assert_str_eq("sha1(" FIPS2_IN ")", cksum.hex, FIPS2_OUT);

	/* it's hard to define a string constant for 1 mil 'a's... */
	sha1_ctx_init(&ctx);
	for (i = 0; i < 1000000; ++i) {
		sha1_ctx_update(&ctx, (unsigned char *)"a", 1);
	}
	sha1_ctx_final(&ctx, cksum.raw);
	sha1_hexdigest(&cksum);
	assert_str_eq("sha1(<1,000,000 x s>)", cksum.hex, FIPS3_OUT);
}

void test_sha1_init()
{
	sha1 calc;
	sha1 init;

	test("SHA1: Initialization");
	sha1_init(&calc, NULL);
	assert_str_eq("sha1()", calc.hex, "");
	/* Borrow from FIPS checks */
	sha1_data(FIPS1_IN, strlen(FIPS1_IN), &calc);
	assert_str_eq("sha1(" FIPS1_IN ")", calc.hex, FIPS1_OUT);

	sha1_init(&init, calc.hex);
	assert_str_eq("init.hex == calc.hex", calc.hex, init.hex);

	unsigned int i;
	char buf[256];
	for (i = 0; i < SHA1_DIGEST_SIZE; i++) {
		snprintf(buf, 256, "octet[%i] equality", i);
		assert_int_eq(buf, calc.raw[i], init.raw[i]);
	}

	test("SHA1: Initialization (valid checksum)");
	sha1_init(&calc, "0123456789abcd0123456789abcd0123456789ab");
	assert_str_ne("valid checksum != ''", calc.hex, "");

	test("SHA1: Initialization (invalid checksum -- to short)");
	sha1_init(&calc, "0123456789abcd0123456789");
	assert_str_eq("short checksum == ''", calc.hex, "");

	test("SHA1: Initialization (invalid checksum -- bad chars)");
	sha1_init(&calc, "0123456789abcd0123456789abcdefghijklmnop");
	assert_str_eq("short checksum == ''", calc.hex, "");
}

void test_sha1_comparison()
{
	sha1 a, b;
	const char *s1 = "This is the FIRST string";
	const char *s2 = "This is the SECOND string";

	sha1_init(&a, NULL);
	sha1_init(&b, NULL);

	sha1_data(s1, strlen(s1), &a);
	sha1_data(s2, strlen(s2), &b);

	assert_int_eq("identical checksums are equal", sha1_cmp(&a, &a), 0);
	assert_int_ne("different checksums are not equal", sha1_cmp(&a, &b), 0);
}

void test_sha1_file()
{
	sha1 cksum;

	test("SHA1: sha1_file");
	assert_int_eq("sha1_file on directory fails", sha1_file("/tmp", &cksum), -1);
	assert_int_eq("sha1_file on non-existent", sha1_file("/tmp/nonexistent", &cksum), -1);
}

void test_suite_sha1()
{
	test_sha1_FIPS();
	test_sha1_init();
	test_sha1_comparison();
	test_sha1_file();
}
