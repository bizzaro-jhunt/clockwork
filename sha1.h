#ifndef SHA1_H
#define SHA1_H

#include <stdint.h>
#include <sys/types.h>

#define SHA1_DIGEST_SIZE 20
#define SHA1_HEX_DIGEST_SIZE 40

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t  buffer[64];
} sha1_ctx;

typedef struct {
	uint8_t raw[SHA1_DIGEST_SIZE];
	char    hex[SHA1_HEX_DIGEST_SIZE + 1];
} sha1;

void sha1_ctx_init(sha1_ctx* context);
void sha1_ctx_update(sha1_ctx* context, const uint8_t* data, const size_t len);
void sha1_ctx_final(sha1_ctx* context, uint8_t digest[SHA1_DIGEST_SIZE]);

void sha1_init(sha1* checksum);
int sha1_fd(int fd, sha1* checksum);
int sha1_file(const char *path, sha1* checksum);
int sha1_data(const void *data, size_t len, sha1* checksum);

#endif /* SHA1_H */
