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

#ifndef CW_SHA1_H
#define CW_SHA1_H

#include <stdint.h>
#include <sys/types.h>

#define CW_SHA1_DIGLEN 20
#define CW_SHA1_HEXLEN 40+1

struct cw_sha1_ctx {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
};

/**
  CW_SHA1 checksum.
 */
struct CW_SHA1 {
	uint8_t raw[CW_SHA1_DIGLEN]; /* raw checksum, for bit comparison */
	char    hex[CW_SHA1_HEXLEN]; /* hex-formatted checksum, for display */
};

void cw_sha1_ctx_init(struct cw_sha1_ctx* context);
void cw_sha1_ctx_update(struct cw_sha1_ctx* context, const uint8_t* data, const size_t len);
void cw_sha1_ctx_final(struct cw_sha1_ctx* context, uint8_t digest[CW_SHA1_DIGLEN]);

void cw_sha1_hexdigest(struct CW_SHA1 *cw_sha1);

void cw_sha1_init(struct CW_SHA1 *checksum, const char *hex);
int cw_sha1_cmp(const struct CW_SHA1* a, const struct CW_SHA1 *b);
int cw_sha1_fd(int fd, struct CW_SHA1* checksum);
int cw_sha1_file(const char *path, struct CW_SHA1* checksum);
int cw_sha1_data(const void *data, size_t len, struct CW_SHA1* checksum);

#endif
