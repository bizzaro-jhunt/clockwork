#ifndef PENDULUM_H
#define PENDULUM_H

#define PENDULUM_VERSION  1

#include <stdint.h>

#define MAX_FUNCTIONS 8192
#define MAX_JUMPS      512

typedef uint8_t  pn_byte;
typedef uint64_t pn_word;
typedef struct pn_machine pn_machine;
typedef pn_word (*pn_function)(pn_machine*);

typedef struct { pn_word op, arg1, arg2; } pn_opcode;
struct pn_machine {
	pn_word A,  B,  C,
	        D,  E,  F;
	pn_word S1, S2;
	pn_word T1, T2, Tr;
	pn_word R,  Er;
	pn_word Ip, Dp;

	FILE *dump_fd;
	int trace;

	size_t     datasize;
	pn_byte   *data;

	size_t     codesize;
	pn_opcode *code;

	struct {
		char        name[32];
		pn_function call;
	} func[MAX_FUNCTIONS];

	struct {
		char label[64];
		pn_word step;
	} jumps[MAX_JUMPS];
};


#define PN_ATTR_DUMPFD 0x0001

int pn_init(pn_machine *m);
int pn_set(pn_machine *m, int attr, void *value);
int pn_func(pn_machine *m, const char *op, pn_function fn);
int pn_parse(pn_machine *m, FILE *io);
int pn_run(pn_machine *m);
int pn_die(pn_machine *m, const char *e);

#endif
