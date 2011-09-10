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

struct sha1_ctx {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
};

/**
  SHA1 checksum.
 */
struct SHA1 {
	uint8_t raw[SHA1_DIGLEN]; /* raw checksum, for bit comparison */
	char    hex[SHA1_HEXLEN]; /* hex-formatted checksum, for display */
};

void sha1_ctx_init(struct sha1_ctx* context);
void sha1_ctx_update(struct sha1_ctx* context, const uint8_t* data, const size_t len);
void sha1_ctx_final(struct sha1_ctx* context, uint8_t digest[SHA1_DIGLEN]);

void sha1_hexdigest(struct SHA1 *sha1);

void sha1_init(struct SHA1 *checksum, const char *hex);
int sha1_cmp(const struct SHA1* a, const struct SHA1 *b);
int sha1_fd(int fd, struct SHA1* checksum);
int sha1_file(const char *path, struct SHA1* checksum);
int sha1_data(const void *data, size_t len, struct SHA1* checksum);

#endif /* SHA1_H */
