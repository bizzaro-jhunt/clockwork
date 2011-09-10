/*
  SHA-1 in C
  Public Domain

  Originally implemented by:
    Steven Reid     <sreid@sea-to-sky.net>
    James H. Brown  <jbrown@burgoyne.com>
    Saul Kravitz    <saul.kravitz@celera.com>
    Ralph Giles     <giles@ghostscript.com>

  Adapted for use in Clockwork by:
    James Hunt <james@jameshunt.us>

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

#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <sys/types.h>

#define SHA1_DIGLEN 20
#define SHA1_HEXLEN 40+1

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} sha1_ctx;

/**
  SHA1 checksum.
 */
typedef struct {
	uint8_t raw[SHA1_DIGLEN]; /* raw checksum, for bit comparison */
	char    hex[SHA1_HEXLEN]; /* hex-formatted checksum, for display */
} sha1;

void sha1_ctx_init(sha1_ctx* context);
void sha1_ctx_update(sha1_ctx* context, const uint8_t* data, const size_t len);
void sha1_ctx_final(sha1_ctx* context, uint8_t digest[SHA1_DIGLEN]);

void sha1_hexdigest(sha1 *sha1);

void sha1_init(sha1* checksum, const char *hex);
int sha1_cmp(const sha1* a, const sha1 *b);
int sha1_fd(int fd, sha1* checksum);
int sha1_file(const char *path, sha1* checksum);
int sha1_data(const void *data, size_t len, sha1* checksum);

#endif /* SHA1_H */
