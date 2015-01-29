/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

#include "../src/vm.h"
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define SHA1_WANT "db726735fb9f56a8f4e9569a0118cc2544c60700"
        /* sha1 of the string "this is what i want.\n" */

#define FAKE_CLOCKD "inproc://clockd"

static void* fake_clockd(void *zmq)
{
	logger(LOG_INFO, "fake_clockd: starting up");
	void *Z = zmq_socket(zmq, ZMQ_ROUTER);
	if (!Z) {
		logger(LOG_EMERG, "fake_clockd: failed to get a ROUTER socket");
		return NULL;
	}

	if (zmq_bind(Z, FAKE_CLOCKD) != 0) {
		logger(LOG_EMERG, "fake_clockd: failed to bind to " FAKE_CLOCKD);
		return NULL;
	}

	pdu_t *pdu;
	logger(LOG_INFO, "fake_clockd: listening for inbound connections");
	while ((pdu = pdu_recv(Z)) != NULL) {
		if (!pdu_type(pdu)) {
			logger(LOG_EMERG, "fake_clockd: empty PDU received");
			return NULL;
		}

		logger(LOG_INFO, "fake_clockd: received a %s", pdu_type(pdu));

		if (strcmp(pdu_type(pdu), "FILE") == 0) { /* should return a 'SHA1' */
			logger(LOG_INFO, "fake_clockd: sending [SHA1][" SHA1_WANT "] response");
			pdu_send_and_free(pdu_reply(pdu, "SHA1", 1, SHA1_WANT), Z);
			continue;
		}

		if (strcmp(pdu_type(pdu), "DATA") == 0) { /* should return BLOCK+ EOF */
			logger(LOG_INFO, "fake_clockd: sending [BLOCK][this is what i want.\\n] response");
			pdu_send_and_free(pdu_reply(pdu, "BLOCK", 1, "this is what i want.\n"), Z);
			pdu = pdu_recv(Z);
			logger(LOG_INFO, "fake_clockd: sending [EOF] response");
			pdu_send_and_free(pdu_reply(pdu, "EOF", 0), Z);
			continue;
		}

		logger(LOG_INFO, "fake_clockd: sending [ERROR][protocol violated] response");
		pdu_send_and_free(pdu_reply(pdu, "ERROR", 1, "protocol violated"), Z);
		return NULL;
	}

	logger(LOG_INFO, "fake_clockd: shutting down");
	return NULL;
}

int main (int argc, char **argv)
{
	int rc;

	log_level(LOG_INFO, NULL);
	log_open("test-pn", "console");
	logger(LOG_INFO, "test-pn starting up");
	assert(argv[1]);

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return 1;
	}
	off_t n = lseek(fd, 0, SEEK_END);
	if (n < 0) {
		perror(argv[1]);
		return 1;
	}
	byte_t *code = mmap(NULL, n, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!code) {
		perror(argv[1]);
		return 1;
	}
	if (!vm_iscode(code, n)) {
		fprintf(stderr, "%s: not a Pendulum bytecode image.\n", argv[1]);
		return 2;
	}

	vm_t vm;
	rc = vm_reset(&vm);
	assert(rc == 0);

	rc = vm_load(&vm, code, n);
	assert(rc == 0);

	char *cfile = string("%s.pcov", argv[1]);
	FILE *io = fopen(cfile, "w");
	if (!io) {
		perror(cfile);
		free(cfile);
		return 1;
	}
	free(cfile);
	vm.ccovio = io;

	rc = vm_args(&vm, argc - 1, argv + 1);
	assert(rc == 0);

	void *zmq = zmq_ctx_new();
	assert(zmq);

	pthread_t tid_zmq;
	rc = pthread_create(&tid_zmq, NULL, fake_clockd, zmq);
	if (rc != 0) {
		perror("pthread_create");
		return 1;
	}

	vm.aux.remote = zmq_socket(zmq, ZMQ_DEALER);
	assert(vm.aux.remote);
	rc = zmq_connect(vm.aux.remote, FAKE_CLOCKD);
	if (rc != 0) {
		perror("zmq_connect");
		return 1;
	}
	sleep_ms(201);

	//vm.trace = 1;
	vm_exec(&vm);

	rc = vm_done(&vm);
	assert(rc == 0);

	void *_;
	pthread_cancel(tid_zmq);
	pthread_join(tid_zmq, &_);
	return 0;
}
