#ifndef VM_H
#define VM_H
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vigor.h>

#include <augeas.h>
#include "authdb.h"

/*

   a BIT is a 0 or a 1
   a NYBLE is 4 bits
   a BYTE is 8 bits
   a WORD is 2 BYTES, 16 BITS
   a DWORD (double word) is 4 BYTES, 32 BITS

   opcode format:

   15                7                 0
   | . . . . . . . . | . . . . . . . . |
   +-----------------+-----------------+
   | r0 |     op     |   t1   |   t2   |
   +-----------------+-----------------+
   |        optional operand1          |
   |               ...                 |
   +-----------------------------------+
   |        optional operand2          |
   |               ...                 |
   +-----------------------------------+

   r0     [2 bits]  RESERVED; must be 00
   op     [6 bits]  opcode value (0-63)
   t1     [4 bits]  type for operand 1
   t2     [4 bits]  type for operand 2
   operands are 32-bit DWORDs

 */

typedef uint8_t   byte_t;
typedef uint16_t  word_t;
typedef uint32_t dword_t;

typedef struct {
	dword_t val[254];
	byte_t  top;
} stack_t;

typedef struct {
	dword_t  addr;
	byte_t  *data;
	size_t   size;
	list_t   l;
} heap_t;

typedef struct {
	size_t     i;
	strings_t *paths;
} dirlist_t;

#define NREGS 16
#define VM_MAX_OPENDIRS 2048
typedef struct {
	dword_t  r[16];  /* generic registers */
	dword_t  acc;    /* accumulator register */
	dword_t  pc;     /* program counter register */
	dword_t  tryc;   /* try counter register */

	byte_t   op, f1, f2;
	dword_t  oper1, oper2;

	const char *topic;
	unsigned int topics;

	stack_t  dstack; /* data stack */
	stack_t  rstack; /* register stack */
	stack_t  istack; /* instruction stack */
	stack_t  tstack; /* "try" nesting stack */

	hash_t   flags;  /* flags (see flag/unflag/flagged? opcodes */
	hash_t   pragma; /* compiler/runtime pragma settings */
	hash_t   props;  /* named properties (version, runtime, etc) */

	list_t   acl;    /* access control list */

	byte_t   trace;  /* whether or not to print trace messages */
	FILE    *stderr; /* where to direct error messages */
	FILE    *stdout; /* where to send normal error messages */
	FILE    *ccovio; /* where to write code coverage info (NULL = don't) */

	byte_t   stop;   /* signal that the execution is complete */

	/* auxiliary */
	struct {
		struct stat   stat;

		augeas       *augeas;

		authdb_t     *authdb;
		user_t       *user;
		group_t      *group;

		struct SHA1   sha1;

		void         *remote;

		dirlist_t     dirs[VM_MAX_OPENDIRS];

		uid_t         runas_uid;
		gid_t         runas_gid;
	} aux;

	list_t   heap;
	dword_t  heaptop;

	dword_t  static0;      /* offset in code[] where static strings start */
	dword_t  codesize;
	byte_t  *code;
} vm_t;

#define HI_NYBLE(_) (((_) >> 4) & 0x0f)
#define LO_NYBLE(_) ( (_)       & 0x0f)
#define HI_BYTE(_)  (((_) >> 8) & 0xff);
#define LO_BYTE(_)  ( (_)       & 0xff);
#define WORD(a,b) ((a << 8) | (b))
#define DWORD(a,b,c,d) ((a << 24) | (b << 16) | (c << 8) | (d))

#define HEAP_ADDRMASK 0x80000000

#define is_value(fl)    ((fl) == TYPE_LITERAL)
#define is_address(fl)  ((fl) == TYPE_ADDRESS)
#define is_register(fl) ((fl) == TYPE_REGISTER)

int vm_iscode(byte_t *code, size_t len);
int vm_reset(vm_t *vm);
int vm_load(vm_t *vm, byte_t *code, size_t len);
int vm_args(vm_t *vm, int argc, char **argv);
int vm_exec(vm_t *vm);
int vm_asm_file(const char *path, byte_t **code, size_t *len, int strip);
int vm_asm_io(FILE *io, byte_t **code, size_t *len, const char *vpath, int strip);
int vm_disasm(vm_t *vm, FILE *out);
int vm_done(vm_t *vm);

#endif
