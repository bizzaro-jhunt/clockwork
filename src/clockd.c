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

int main(int argc, char **argv)
{
	struct manifest *manifest = parse_file("/cfm/etc/manifest.pol");
	if (!manifest) exit(1);

	void *context  = zmq_ctx_new();
	void *listener = zmq_socket(context, ZMQ_ROUTER);
	zmq_bind(listener, "tcp://*:2323");
	cw_log_open("clockd", "stdout");
	cw_log(LOG_INFO, "clockd starting up");

	while (!cw_sig_interrupt()) {
		cw_pdu_t *pdu, *reply;
		pdu = cw_pdu_recv(listener);
		assert(pdu);

		if (strcmp(pdu->type, "POLICY") == 0) {
			char *name    = cw_pdu_text(pdu, 1);
			char *factstr = cw_pdu_text(pdu, 2);
			cw_log(LOG_INFO, "inbound policy request for %s", name);
			cw_log(LOG_INFO, "facts are: %s", factstr);

			struct hash *facts = hash_new();
			fact_read_string(factstr, facts);

			struct stree *pnode = hash_get(manifest->hosts, name);
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
