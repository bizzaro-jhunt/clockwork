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
#define PROTOCOL_VERSION         1
#define PROTOCOL_VERSION_STRING "1"

typedef enum {
	STATE_INIT,
	STATE_IDENTIFIED,
	STATE_POLICY,
	STATE_FILE,
	STATE_REPORT,
} state_t;

static const char *FSM_STATES[] = {
	"INIT",
	"IDENTIFIED",
	"POLICY",
	"FILE",
	"REPORT",
	NULL,
};

typedef enum {
	EVENT_UNKNOWN,
	EVENT_PING,
	EVENT_HELLO,
	EVENT_POLICY,
	EVENT_FILE,
	EVENT_DATA,
	EVENT_REPORT,
	EVENT_BYE,
} event_t;

static const char *FSM_EVENTS[] = {
	"UNKNOWN",
	"PING",
	"HELLO",
	"POLICY",
	"FILE",
	"DATA",
	"REPORT",
	"BYE",
	NULL,
};

static event_t s_pdu_event(cw_pdu_t *pdu)
{
	int i;
	for (i = 0; FSM_EVENTS[i]; i++)
		if (strcmp(pdu->type, FSM_EVENTS[i]) == 0)
			return (event_t)i;
	return EVENT_UNKNOWN;
}

typedef enum {
	FSM_ERR_SUCCESS,
	FSM_ERR_INTERNAL,
	FSM_ERR_NOT_IMPLEMENTED,
	FSM_ERR_NO_POLICY_FOUND,
	FSM_ERR_NO_RESOURCE_FOUND,
	FSM_ERR_BADPROTO,
} fsm_err_t;

static const char *FSM_ERRORS[] = {
	"Success",
	"Internal Error",
	"Not Implemented",
	"Policy Not Found",
	"Resource Not Found",
	"Protocol Violation",
	NULL,
};

typedef struct {
	state_t           state;
	event_t           event;
	fsm_err_t         error;

	cw_frame_t       *ident;
	int32_t           last_seen;

	char             *name;
	struct manifest  *manifest;
	struct stree     *pnode;
	struct policy    *policy;
	cw_hash_t        *facts;

	FILE             *io;
	unsigned long     offset;
	struct SHA1       sha1;

	void             *mapped;
	size_t            maplen;
} client_t;

static int s_sha1(client_t *fsm)
{
	struct sha1_ctx ctx;
	sha1_init(&fsm->sha1, NULL);
	sha1_ctx_init(&ctx);

	char data[BLOCK_SIZE];
	size_t n;
	int fd = fileno(fsm->io);
	while ((n = read(fd, data, BLOCK_SIZE)) > 0)
		sha1_ctx_update(&ctx, (uint8_t *)data, n);
	sha1_ctx_final(&ctx, fsm->sha1.raw);
	sha1_hexdigest(&fsm->sha1);

	rewind(fsm->io);
	return 0;
}

static char * s_gencode(client_t *c)
{
	c->policy = policy_generate(c->pnode, c->facts);

	FILE *io = tmpfile();
	assert(io);

	policy_gencode(c->policy, io);
	fprintf(io, "%c", '\0');
	c->maplen = ftell(io);
	c->mapped = mmap(NULL, c->maplen, PROT_READ, MAP_SHARED, fileno(io), 0);
	fclose(io);

	if (c->mapped == MAP_FAILED)
		c->mapped = NULL;

	return (char *)c->mapped;
}

static int s_state_machine(client_t *fsm, cw_pdu_t *pdu, cw_pdu_t **reply)
{

	fsm->last_seen = cw_time_s();

	cw_log(LOG_INFO, "fsm: transition %s [%i] -> %s [%i]",
			FSM_STATES[fsm->state], fsm->state,
			FSM_EVENTS[fsm->event], fsm->event);

	switch (fsm->event) {
	case EVENT_UNKNOWN:
		fsm->error = FSM_ERR_BADPROTO;
		return 1;

	case EVENT_PING:
		*reply = cw_pdu_make(pdu->src, 2, "PONG", PROTOCOL_VERSION_STRING);
		return 0;

	case EVENT_HELLO:
		switch (fsm->state) {
		case STATE_FILE:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
			cw_hash_done(fsm->facts, 0);
			fsm->facts = NULL;
			policy_free(fsm->policy);
			fsm->policy = NULL;

		case STATE_IDENTIFIED:
			free(fsm->name);

		case STATE_INIT:
		case STATE_REPORT:
			/* fall-through */
			break;
		}

		/* FIXME: send back pendulum code for gatherer setup */
		fsm->name = cw_pdu_text(pdu, 1);
		*reply = cw_pdu_make(pdu->src, 1, "OK");

		fsm->state = STATE_IDENTIFIED;
		return 0;

	case EVENT_POLICY:
		switch (fsm->state) {
		case STATE_INIT:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_REPORT:
		case STATE_FILE:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
			cw_hash_done(fsm->facts, 0);
			fsm->facts = NULL;
			policy_free(fsm->policy);
			fsm->policy = NULL;

		case STATE_IDENTIFIED:
			/* fall-through */
			break;
		}

		char *facts = cw_pdu_text(pdu, 2);
		fsm->facts = cw_alloc(sizeof(cw_hash_t));
		fact_read_string(facts, fsm->facts);
		free(facts);

		fsm->pnode = cw_hash_get(fsm->manifest->hosts, fsm->name);
		if (!fsm->pnode) fsm->pnode = fsm->manifest->fallback;
		if (!fsm->pnode) {
			fsm->error = FSM_ERR_NO_POLICY_FOUND;
			return 1;
		}
		fsm->policy = policy_generate(fsm->pnode, fsm->facts);

		char *code = s_gencode(fsm);
		*reply = cw_pdu_make(pdu->src, 2, "POLICY", code);
		fsm->state = STATE_POLICY;
		return 0;

	case EVENT_FILE:
		switch (fsm->state) {
		case STATE_INIT:
		case STATE_IDENTIFIED:
		case STATE_REPORT:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_FILE:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
			/* fall-through */
			break;
		}

		/* open the file, calculate the SHA1 */
		char *key = cw_pdu_text(pdu, 1);
		struct resource *r = cw_hash_get(fsm->policy->index, key);
		free(key);

		if (!r) {
			fsm->error = FSM_ERR_NO_RESOURCE_FOUND;
			return 1;
		}

		fsm->io = resource_content(r, fsm->facts);
		if (!fsm->io) {
			fsm->error = FSM_ERR_INTERNAL;
			return 1;
		}

		s_sha1(fsm);
		*reply = cw_pdu_make(pdu->src, 2, "SHA1", fsm->sha1.hex);
		fsm->state = STATE_FILE;
		return 0;

	case EVENT_DATA:
		switch (fsm->state) {
		case STATE_INIT:
		case STATE_IDENTIFIED:
		case STATE_POLICY:
		case STATE_REPORT:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_FILE:
			/* fall-through */
			break;
		}

		char *off = cw_pdu_text(pdu, 1);
		fsm->offset = BLOCK_SIZE * atoi(off);
		free(off);

		char block[BLOCK_SIZE+1] = "";
		fseek(fsm->io, fsm->offset, SEEK_SET);
		size_t n = fread(block, 1, BLOCK_SIZE, fsm->io);
		block[n] = '\0';

		if (n == 0) {
			if (feof(fsm->io)) {
				*reply = cw_pdu_make(pdu->src, 1, "EOF");
			} else {
				cw_log(LOG_ERR, "Failed to read from cached IO handled: %s", strerror(errno));
				*reply = cw_pdu_make(pdu->src, 2, "ERROR", "read error");
			}

		} else {
			*reply = cw_pdu_make(pdu->src, 2, "BLOCK", block);
		}
		return 0;

	case EVENT_REPORT:
		switch (fsm->state) {
		case STATE_INIT:
		case STATE_IDENTIFIED:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_FILE:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
		case STATE_REPORT:
			/* fall-through */
			break;
		}
		break;

	case EVENT_BYE:
		switch (fsm->state) {
		case STATE_REPORT:
		case STATE_FILE:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
			cw_hash_done(fsm->facts, 0);
			fsm->facts = NULL;
			policy_free(fsm->policy);
			fsm->policy = NULL;

		case STATE_IDENTIFIED:
			free(fsm->name);
			fsm->name = NULL;

		case STATE_INIT:
			/* fall-through */
			break;
		}

		fsm->state = STATE_INIT;
		fsm->last_seen = 1;
		*reply = cw_pdu_make(pdu->src, 1, "BYE");
		return 0;
	}

	fsm->error = FSM_ERR_INTERNAL;
	return 1;
}

typedef struct {
	size_t len;
	int32_t min_life;
	client_t clients[];
} ccache_t;

ccache_t* s_ccache_init(size_t num_conns, int32_t min_life)
{
	ccache_t *cc = cw_alloc(
			sizeof(ccache_t) +
			(num_conns * sizeof(client_t)));
	cc->len      = num_conns;
	cc->min_life = min_life;
	return cc;
}
static void s_ccache_purge(ccache_t *c)
{
	int32_t now = cw_time_s();
	int n = 0;
	size_t i;
	for (i = 0; i < c->len; i++) {
		if (c->clients[i].last_seen == 0
		 || c->clients[i].last_seen >= now - c->min_life)
			continue;

		cw_log(LOG_DEBUG, "purging %p, last_seen=%i, name=%s, %s facts, %s policy",
			c->clients[i], c->clients[i].last_seen, c->clients[i].name,
			(c->clients[i].facts  ? "has" : "no"),
			(c->clients[i].policy ? "has" : "no"));
		cw_log(LOG_INFO, "purging %p, last_seen=%i",
			&c->clients[i], c->clients[i].last_seen);

		free(c->clients[i].ident);
		c->clients[i].ident = NULL;

		free(c->clients[i].name);
		c->clients[i].name = NULL;

		if (c->clients[i].facts)
			cw_hash_done(c->clients[i].facts, 0);

		if (c->clients[i].io)
			fclose(c->clients[i].io);
		c->clients[i].io        = NULL;
		c->clients[i].offset    = 0;
		c->clients[i].last_seen = 0;
		n++;
	}
	cw_log(LOG_INFO, "purged %i ccache entries", n);
}
static size_t s_ccache_index(cw_frame_t *ident, ccache_t *c)
{
	size_t i;
	for (i = 0; i < c->len; i++)
		if (c->clients[i].ident
		 && cw_frame_cmp(ident, c->clients[i].ident))
			break;
	return i;
}
static size_t s_ccache_next(ccache_t *c)
{
	size_t i;
	for (i = 0; i < c->len; i++)
		if (!c->clients[i].ident)
			break;
	return i;
}
static client_t* s_ccache_find(cw_frame_t *ident, ccache_t *c)
{
	size_t i = s_ccache_index(ident, c);
	if (i >= c->len) {
		i = s_ccache_next(c);
		if (i >= c->len)
			return NULL;
		c->clients[i].ident = cw_frame_copy(ident);
		c->clients[i].io = NULL;
		c->clients[i].offset = 0;
	}
	c->clients[i].last_seen = cw_time_s();
	return &c->clients[i];
}

int main(int argc, char **argv)
{
	struct manifest *manifest = parse_file("test.pol");
	if (!manifest) exit(1);

	cw_log_open("clockd", "stdout");
	cw_log_level(0, "info");

	cw_log(LOG_INFO, "clockd starting up");
	void *context  = zmq_ctx_new();
	void *listener = zmq_socket(context, ZMQ_ROUTER);
	cw_log(LOG_INFO, "binding to tcp://*:2323");
	zmq_bind(listener, "tcp://*:2323");

	ccache_t *cc = s_ccache_init(2048, 600);

	while (!cw_sig_interrupt()) {
		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_recv(listener);
		assert(pdu);

		s_ccache_purge(cc);
		client_t *c = s_ccache_find(pdu->src, cc);
		if (!c) {
			cw_log(LOG_CRIT, "max connections reached!");
			reply = cw_pdu_make(pdu->src, 2, "ERROR", "Server busy; try again later\n");
			cw_pdu_send(listener, reply);
			continue;
		}

		c->manifest = manifest;
		c->event = s_pdu_event(pdu);
		cw_log(LOG_INFO, "%p: incoming connection", c);
		int rc = s_state_machine(c, pdu, &reply);
		if (rc == 0) {
			cw_log(LOG_INFO, "%p: fsm is now at %s [%i]", c, FSM_STATES[c->state], c->state);
			cw_log(LOG_INFO, "%p: sending back a %s PDU", c, reply->type);
			cw_pdu_send(listener, reply);

			if (c->mapped) {
				munmap(c->mapped, c->maplen);
				c->mapped = NULL;
				c->maplen = 0;
			}

		} else {
			reply = cw_pdu_make(pdu->src, 2, "ERROR", FSM_ERRORS[c->error]);
			cw_log(LOG_INFO, "%p: sending back an ERROR PDU: %s", c, FSM_ERRORS[c->error]);
			cw_pdu_send(listener, reply);
		}
	}
	cw_log(LOG_INFO, "shutting down");

	cw_zmq_shutdown(listener, 500);
	zmq_ctx_destroy(context);
	return 0;
}
