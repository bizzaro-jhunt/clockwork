#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <sys/types.h>

/** @file sha1.h

  This module defines functions for generating SHA1 checksums
  from files, strings and file descriptors.

  The implementation is based on the public domain implementation
  originally written by Steven Reid, and updated by collaborative
  volunteers on the internet.
 */

/** @cond false */
#define SHA1_DIGEST_SIZE 20
#define SHA1_HEX_DIGEST_SIZE 40

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} sha1_ctx;
/** @endcond */

/** SHA1 checksum. */
typedef struct {
	/** Raw 20-octet SHA1 checksum, useful for comparison. */
	uint8_t raw[SHA1_DIGEST_SIZE];
	/** Hex-formatted SHA1 checksum, useful for display / logging. */
	char    hex[SHA1_HEX_DIGEST_SIZE + 1];
} sha1;

/** @cond false */
void sha1_ctx_init(sha1_ctx* context);
void sha1_ctx_update(sha1_ctx* context, const uint8_t* data, const size_t len);
void sha1_ctx_final(sha1_ctx* context, uint8_t digest[SHA1_DIGEST_SIZE]);

void sha1_hexdigest(sha1 *sha1);
/** @endcond */

/**
  Initialize a SHA1 checksum structure.

  This function ensures that the sha1::raw and sha1::hex members
  are initialized to appropriate starting values.  Callers wishing
  to use the helper methods like sha1_file and sha1_data do not
  need to call this function explicitly.

  If \a hex is specified, then \a checksum will be initialized so
  that the hex member is a copy of \a hex, and the raw member
  contains the raw checksum that \a hex represents.

  @param  checksum    Pointer to a sha1 to initialize.
  @param  hex         Hex-encoded SHA1 checksum (optional)
 */
void sha1_init(sha1* checksum, const char *hex);

/**
  Calculate the SHA1 checksum of data read from a file descriptor.

  Data will be read until \a fd returns an EOF condition.

  This function calculates both the raw and hex representation of
  the SHA1 cecksum.

  @param  fd          File descriptor to read from.
  @param  checksum    Pointer to a sha1 to store the checksum in.

  @returns 0 on success, non-zero on failure.
 */
int sha1_fd(int fd, sha1* checksum);

/**
  Calculate the SHA1 checksum of a file.

  The file will be opened in read-only mode and read in chunks
  to calculate the checksum.  If the file cannot be opened
  (permission denied, ENOENT, etc.) non-zero will be returned to
  indicate failure.

  This function calculates both the raw and hex representation of
  the SHA1 cecksum.

  @param  path        Path to the file to checksum.
  @param  checksum    Pointer to a sha1 to store the checksum in.

  @returns 0 on success, non-zero on failure.
 */
int sha1_file(const char *path, sha1* checksum);

/**
  Calculate the SHA1 checksum of a fixed-length buffer.

  \a data will be treated as an array of \a len 8-bit unsigned
  integers.  Character strings will be converted according to
  standard C locale rules.

  This function calculates both the raw and hex representation of
  the SHA1 cecksum.

  @param  data        Buffer containing the data to checksum.
  @param  len         Length of the \a data buffer.
  @param  checksum    Pointer to a sha1 to store the checksum in.

  @returns 0 on success, non-zero on failure.
 */
int sha1_data(const void *data, size_t len, sha1* checksum);

#endif /* SHA1_H */
