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

#include "clockwork.h"
#include <zmq.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libgen.h>

#include "spec/parser.h"
#include "resources.h"

#define BLOCK_SIZE 8192

#if 0
#define FILE_DIR "/tmp/clockd"
struct file {
	struct SHA1  sha1;
	int32_t      last_seen;
};
struct file_cache {
	size_t        len;
	struct file  *files;
};
#endif
static FILE* s_render(const char *source, cw_hash_t *facts)
{
	FILE *in  = tmpfile();
	FILE *out = tmpfile();

	if (!in || !out) {
		fclose(in);
		fclose(out);
		return NULL;
	}

	char *k, *v;
	for_each_key_value(facts, k, v)
		fprintf(in, "%s=%s\n", k, v);
	fseek(in, 0, SEEK_SET);

	int rc = cw_run2(in, out, NULL, "./template-erb", source);
	fclose(in);

	if (rc == 0)
		return out;

	fclose(out);
	return NULL;
}
#if 0
static char* s_sha1path(struct SHA1 *sha1)
{
	char buf[4] = {0};
	memcpy(buf, sha1->hex, 3);
	return cw_string("%s/%s/%s", FILE_DIR, buf, sha1->hex);
}
static size_t s_fcache_next(struct file_cache *c)
{
	int32_t oldest = cw_time_s();
	size_t i, j;
	for (i = 0, j = 0; i < c->len; i++)
		if (c->files[i].last_seen < oldest)
			j = i, oldest = c->files[i].last_seen;

	if (c->files[j].last_seen) {
		/* purge */
		c->files[j].last_seen = 0;
		char *path = s_sha1path(c->files[j].sha1);
		unlink(path);
		free(path);
	}
	return j;
}
static int s_fcache_save(FILE *io, struct SHA1 *sha1, struct file_cache *c)
{
	/* store the file in the cache dir */
	char *path = s_sha1path(sha1);
	FILE *out = fopen(path, "w");
	free(path);

	if (!out)
		return -1;

	char buf[BLOCK_SIZE];
	size_t n;
	fseek(io, 0, SEEK_SET);
	while ((n = fread(buf, BLOCK_SIZE, sizeof(char), io)) > 0)
		fwrite(buf, BLOCK_SIZE, sizeof(char), out);
	fclose(out);

	/* update the file cache structure */
	size_t i = s_fcacheslot(c);
	c->files[i].last_seen = cw_time_s();
	memcpy(&c->files[i].sha1, sha1, sizeof(struct SHA1));
	return 0;
}

static size_t s_fcache_find(struct SHA1 *sha1, struct file_cache *c)
{
	size_t i;
	for (i = 0; i < c->len; i++) {
		if (c->files[i].last_seen == 0) continue;
		if (memcmp(c->files[i].sha1.raw, sha1->raw, SHA1_DIGLEN) == 0) break;
	}
	return i;
}
#endif
struct conn {
	cw_frame_t *ident;
	int32_t last_seen;
	FILE *io;
	long offset;
};
struct conn_cache {
	size_t len;
	int32_t min_life;
	struct conn connections[];
};

struct conn_cache* s_ccache_init(size_t num_conns, int32_t min_life)
{
	struct conn_cache *cc = cw_alloc(
			sizeof(struct conn_cache) +
			(num_conns * sizeof(struct conn)));
	cc->len      = num_conns;
	cc->min_life = min_life;
	return cc;
}
static void s_ccache_purge(struct conn_cache *c)
{
	int32_t now = cw_time_s();
	size_t i;
	for (i = 0; i < c->len; i++) {
		if (c->connections[i].last_seen == 0
		 || c->connections[i].last_seen >= now - c->min_life)
			continue;

		if (c->connections[i].io)
			fclose(c->connections[i].io);
		c->connections[i].io        = NULL;
		c->connections[i].offset    = 0;
		c->connections[i].last_seen = 0;
	}
}
static size_t s_ccache_find(cw_frame_t *ident, struct conn_cache *c)
{
	size_t i;
	for (i = 0; i < c->len; i++)
		if (c->connections[i].ident
		 && cw_frame_cmp(ident, c->connections[i].ident) == 0)
			break;
	return i;
}
static size_t s_ccache_next(struct conn_cache *c)
{
	size_t i;
	for (i = 0; i < c->len; i++)
		if (!c->connections[i].ident)
			break;
	return i;
}
static struct conn* s_ccache_conn(cw_frame_t *ident, struct conn_cache *c)
{
	size_t i = s_ccache_find(ident, c);
	if (i >= c->len) {
		i = s_ccache_next(c);
		if (i >= c->len)
			return NULL;
		c->connections[i].ident = cw_frame_copy(ident);
		c->connections[i].io = NULL;
		c->connections[i].offset = 0;
	}
	return &c->connections[i];
}

int main(int argc, char **argv)
{
	struct manifest *manifest = parse_file("test.pol");
	if (!manifest) exit(1);

	void *context  = zmq_ctx_new();
	void *listener = zmq_socket(context, ZMQ_ROUTER);
	zmq_bind(listener, "tcp://*:2323");
	cw_log_open("clockd", "stdout");
	cw_log(LOG_INFO, "clockd starting up");

	struct conn_cache *cc = s_ccache_init(2048, 600);

	while (!cw_sig_interrupt()) {
		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_recv(listener);
		assert(pdu);

		s_ccache_purge(cc);
		struct conn *conn = s_ccache_conn(pdu->src, cc);
		if (!conn) {
			cw_log(LOG_CRIT, "max connections reached!");
			reply = cw_pdu_make(pdu->src, 2, "ERROR", "Server busy; try again later\n");
			cw_pdu_send(listener, reply);

		} else if (strcmp(pdu->type, "POLICY") == 0) {
			char *name    = cw_pdu_text(pdu, 1);
			char *factstr = cw_pdu_text(pdu, 2);
			cw_log(LOG_INFO, "inbound policy request for %s", name);
			cw_log(LOG_INFO, "facts are: %s", factstr);

			cw_hash_t *facts = cw_alloc(sizeof(cw_hash_t));
			fact_read_string(factstr, facts);

			struct stree *pnode = cw_hash_get(manifest->hosts, name);
			if (!pnode)
				pnode = manifest->fallback;
			if (!pnode) {
				cw_log(LOG_ERR, "no policy found for %s", name);
				reply = cw_pdu_make(pdu->src, 2, "ERROR", "No suitable policy found\n");
				cw_pdu_send(listener, reply);

			} else {
				struct policy *policy = policy_generate(pnode, facts);

				FILE *io = tmpfile();
				assert(io);
				int rc = policy_gencode(policy, io);
				assert(rc == 0);
				fprintf(io, "%c", '\0');
				size_t len = ftell(io);

				char *code = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(io), 0);

				reply = cw_pdu_make(pdu->src, 2, "OK", code);
				cw_pdu_send(listener, reply);

				munmap(code, len);
				fclose(io);
			}

		} else if (strcmp(pdu->type, "FILE") == 0) {
			char *name    = cw_pdu_text(pdu, 1);
			char *factstr = cw_pdu_text(pdu, 2);
			char *source  = cw_pdu_text(pdu, 3);
			cw_log(LOG_INFO, "inbound file request for %s", name);
			cw_log(LOG_INFO, "facts are: %s", factstr);

			cw_hash_t *facts = cw_alloc(sizeof(cw_hash_t));
			fact_read_string(factstr, facts);

			FILE *io = s_render(source, facts);
			if (!io) {
				reply = cw_pdu_make(pdu->src, 2, "ERROR", "internal error");
				cw_pdu_send(listener, reply);

			} else {
				fseek(io, 0, SEEK_SET);
				if (conn->io) fclose(conn->io);
				conn->io = io;
				conn->offset = 0;

				reply = cw_pdu_make(pdu->src, 1, "OK");
				cw_pdu_send(listener, reply);
			}

		} else if (strcmp(pdu->type, "MORE") == 0) {

			if (!conn->io) {
				reply = cw_pdu_make(pdu->src, 2, "ERROR", "protocol violation");
				cw_pdu_send(listener, reply);

			} else {
				fseek(conn->io, conn->offset, SEEK_SET);
				char buf[BLOCK_SIZE+1];
				size_t n = fread(buf, BLOCK_SIZE+1, sizeof(char), conn->io);
				buf[n] = '\0';
				conn->offset = ftell(conn->io);

				reply = cw_pdu_make(pdu->src, 2, "DATA", buf);
				cw_pdu_send(listener, reply);
			}

		} else {
			cw_log(LOG_INFO, "unrecognized PDU: '%s'", pdu->type);
			reply = cw_pdu_make(pdu->src, 2, "ERROR", "Unrecognized PDU");
			cw_pdu_send(listener, reply);
		}
	}
	cw_log(LOG_INFO, "shutting down");

	cw_zmq_shutdown(listener, 500);
	zmq_ctx_destroy(context);
	return 0;
}
