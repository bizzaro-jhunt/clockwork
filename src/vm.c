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

#include "vm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <augeas.h>

#define OPCODES_EXTENDED
#include "opcodes.h"
#include "authdb.h"
#include "mesh.h"

static int s_empty(stack_t *st)
{
	return st->top == 0;
}

static int s_push(vm_t *vm, stack_t *st, dword_t value)
{
	assert(st);
	if (st->top == 254) {
		fprintf(vm->stderr, "stack overflow!\n");
		vm->abort = 1;
		return 0;
	}

	st->val[st->top++] = value;
	return 0;
}

static dword_t s_pop(vm_t *vm, stack_t *st)
{
	assert(st);
	if (s_empty(st)) {
		fprintf(vm->stderr, "stack underflow!\n");
		vm->abort = 1;
		return 0;
	}

	return st->val[--st->top];
}

static void *s_ptr(vm_t *vm, dword_t arg)
{
	heap_t *h;

	if (arg & HEAP_ADDRMASK) {
		for_each_object(h, &vm->heap, l) {
			if (h->addr == arg) {
				return h->data;
			}
		}
	} else if (arg < vm->codesize) {
		return (void *)(vm->code + arg);
	}
	return NULL;
}

static const char *s_str(vm_t *vm, byte_t type, dword_t arg)
{
	switch (type) {
	case TYPE_ADDRESS:  return (char *)s_ptr(vm, arg);
	case TYPE_REGISTER: return (char *)s_ptr(vm, vm->r[arg]);
	default:            return NULL;
	}
}

static dword_t s_val(vm_t *vm, byte_t type, dword_t arg)
{
	dword_t bad = 0x40000000;
	switch (type) {
	case TYPE_LITERAL:
		return arg;

	case TYPE_ADDRESS:
		if (arg & HEAP_ADDRMASK) return arg;
		if (arg >= vm->codesize) return bad;
		return arg;

	case TYPE_REGISTER:
		if (arg > NREGS) return bad;
		return vm->r[arg];

	default:
		return bad;
	}
}

static void s_save_state(vm_t *vm)
{
	int i;
	for (i = 0; i < NREGS; i++)
		s_push(vm, &vm->dstack, vm->r[i]);
}

static void s_restore_state(vm_t *vm)
{
	int i;
	for (i = NREGS - 1; i >= 0; i--)
		vm->r[i] = s_pop(vm, &vm->dstack);
}

static char HEX[] = "0123456789abcdef";
static char __bin[64 * 3 + 1];
static char *bin(byte_t *data, size_t size)
{
	char *s = __bin;
	size_t i;
	for (i = 0; i < size && i < 64; i++) {
		*s++ = HEX[(*data & 0xf0) >> 4];
		*s++ = HEX[(*data & 0x0f)];
		*s++ = ' ';
		data++;
	}
	*s = '\0';
	return __bin;
}

static heap_t *vm_heap_alloc(vm_t *vm, size_t n)
{
	heap_t *h = calloc(1, sizeof(heap_t));
	h->addr = vm->heaptop++ | HEAP_ADDRMASK;
	h->size = n;
	if (n)
		h->data = calloc(n, sizeof(byte_t));
	list_push(&vm->heap, &h->l);
	return h;
}

static dword_t vm_heap_strdup(vm_t *vm, const char *s)
{
	heap_t *h = calloc(1, sizeof(heap_t));
	h->addr = vm->heaptop++ | HEAP_ADDRMASK;
	h->size = strlen(s) + 1;
	h->data = (byte_t*)strdup(s);
	list_push(&vm->heap, &h->l);
	return h->addr;
}

static void dump(FILE *io, vm_t *vm)
{
	fprintf(io, "\n");
	fprintf(io, "    ---------------------------------------------------------------------\n");
	fprintf(io, "    %%%c [ %08x ]   %%%c [ %08x ]   %%%c [ %08x ]   %%%c [ %08x ]\n",
		'a', vm->r[0],  'b', vm->r[1], 'c', vm->r[2],  'd', vm->r[3]);
	fprintf(io, "    %%%c [ %08x ]   %%%c [ %08x ]   %%%c [ %08x ]   %%%c [ %08x ]\n",
		'e', vm->r[4],  'f', vm->r[5], 'g', vm->r[6],  'h', vm->r[7]);
	fprintf(io, "    %%%c [ %08x ]   %%%c [ %08x ]   %%%c [ %08x ]   %%%c [ %08x ]\n",
		'i', vm->r[8],  'j', vm->r[9], 'k', vm->r[10], 'l', vm->r[11]);
	fprintf(io, "    %%%c [ %08x ]   %%%c [ %08x ]   %%%c [ %08x ]   %%%c [ %08x ]\n",
		'm', vm->r[12], 'n', vm->r[13], 'o', vm->r[14], 'p', vm->r[15]);
	fprintf(io, "\n");

	fprintf(io, "    acc: %08x\n", vm->acc);
	fprintf(io, "     pc: %08x\n", vm->pc);
	fprintf(io, "\n");

	int i;
	if (vm->dstack.top == 0) {
		fprintf(io, "    data: <s_empty>\n");
	} else {
		fprintf(io, "    data: | %08x | 0\n", vm->dstack.val[0]);
		for (i = 1; i < vm->dstack.top; i++)
			fprintf(io, "          | %08x | %i\n", vm->dstack.val[i], i);
	}

	if (vm->istack.top == 0) {
		fprintf(io, "    inst: <s_empty>\n");
	} else {
		fprintf(io, "    inst: | %08x | 0\n", vm->istack.val[0]);
		for (i = 1; i < vm->istack.top; i++)
			fprintf(io, "          | %08x | %u\n", vm->istack.val[i], i);
	}

	if (vm->heaptop != 0) {
		fprintf(io, "    heap:\n");
		heap_t *h;
		for_each_object(h, &vm->heap, l)
			fprintf(io, "          [%s] %u\n", bin(h->data, h->size), h->addr - HEAP_ADDRMASK);
	}

	fprintf(io, "    ---------------------------------------------------------------------\n\n");
}

static int ischar(char c, const char *accept)
{
	assert(accept);
	while (*accept && *accept != c) accept++;
	return *accept == c;
}

static char *_sprintf(vm_t *vm, const char *fmt)
{
	assert(vm);
	assert(fmt);

	size_t n;
	char reg, type, next, *a, *b, *buf, *s, *s1 = NULL;

#define ADVANCE do { \
	b++; \
	if (!*b) { \
		fprintf(stderr, "<< unexpected end of format string >>\n"); \
		free(buf); return NULL; \
	} \
} while (0)

	a = b = buf = strdup(fmt);
	n = 0;
	while (*b) {
		if (*b == '%') {
			ADVANCE;
			if (*b == '%') {
				n++;
				ADVANCE;
				continue;
			}
			if (*b != '[') {
				fprintf(stderr, "<< %s >>\n", fmt);
				fprintf(stderr, "<< invalid format specifier, '[' != '%c', offset:%li >>\n", *b, b - a + 1);
				goto bail;
			}

			ADVANCE;
			if (*b < 'a' || *b >= 'a' + NREGS) {
				fprintf(stderr, "<< %s >>\n", fmt);
				fprintf(stderr, "<< invalid register %%%c, offset:%li >>\n", *b, b - a + 1);
				goto bail;
			}
			reg = *b;

			ADVANCE;
			if (*b != ']') {
				fprintf(stderr, "<< %s >>\n", fmt);
				fprintf(stderr, "<< invalid format specifier, ']' != '%c', offset:%li >>\n", *b, b - a + 1);
				goto bail;
			}
			a = b; *a = '%';

			while (*b && !ischar(*b, "sdiouxX"))
				ADVANCE;
			type = *b; b++;
			next = *b; *b = '\0';

			switch (type) {
			case 's':
				n += snprintf(NULL, 0, a, s_str(vm, TYPE_REGISTER, reg - 'a'));
				break;
			default:
				n += snprintf(NULL, 0, a, vm->r[reg - 'a']);
				break;
			}

			*b = next;
			a = b;
			continue;
		}
		b++;
		n++;
	}

	s = s1 = calloc(n + 1, sizeof(char));
	a = b = buf;
	while (*b) {
		if (*b == '%') {
			if (*(b+1) == '%') { /* %% == % */
				*s++ = '%';
				ADVANCE;
				ADVANCE;
				a = b;
				continue;
			}
			*b = '\0';
			ADVANCE; /* [    */
			ADVANCE; /*  %r  */  reg = *b;
			ADVANCE; /*    ] */  a = b; *a = '%';

			while (*b && !ischar(*b, "sdiouxX"))
				ADVANCE;
			type = *b; b++;
			next = *b; *b = '\0';

			switch (type) {
			case 's':
				s += snprintf(s, n - (s - s1) + 1, a, s_str(vm, TYPE_REGISTER, reg - 'a'));
				break;
			default:
				s += snprintf(s, n - (s - s1) + 1, a, vm->r[reg - 'a']);
				break;
			}

			*b = next;
			a = b;
			continue;
		}
		*s++ = *b++;
	}
#undef ADVANCE

bail:
	free(buf);
	return s1;
}

static dword_t vm_sprintf(vm_t *vm, const char *fmt)
{
	assert(vm);
	assert(fmt);

	char *s = _sprintf(vm, fmt);
	heap_t *h = vm_heap_alloc(vm, 0);
	h->data = (byte_t *)s;
	h->size = strlen(s + 1);
	return h->addr;
}

static void vm_fprintf(vm_t *vm, FILE *out, const char *fmt)
{
	assert(vm);
	assert(out);
	assert(fmt);

	char *s = _sprintf(vm, fmt);
	fprintf(out, "%s", s);
	free(s);
	return;
}

static void s_aug_perror(FILE *io, augeas *aug)
{
	char **err = NULL;
	const char *value;
	int i, rc = aug_match(aug, "/augeas//error", &err);
	fprintf(io, ": found %u problem%s:\n", rc, rc == 1 ? "" : "s");
	for (i = 0; i < rc; i++) {
		aug_get(aug, err[i], &value);
		fprintf(io, "  %s: %s\n", err[i], value);
	}
	free(err);
}

static char * s_aug_find(augeas *aug, const char *needle)
{
	char **r = NULL, *v;
	int rc = aug_match(aug, needle, &r);
	if (rc == 1) {
		v = r[0];
		free(r);
		return v;
	}
	while (rc) free(r[--rc]);
	free(r);
	return NULL;
}

/************************************************************************/

int vm_reset(vm_t *vm)
{
	assert(vm);
	memset(vm, 0, sizeof(vm_t));
	list_init(&vm->heap);
	return 0;
}

int vm_prime(vm_t *vm, byte_t *code, size_t len)
{
	assert(vm);
	assert(code);
	assert(len > 1);

	/* default pragmas */
	hash_set(&vm->pragma, "authdb.root",  strdup(AUTHDB_ROOT));
	hash_set(&vm->pragma, "augeas.root",  strdup(AUGEAS_ROOT));
	hash_set(&vm->pragma, "augeas.libs",  strdup(AUGEAS_LIBS));
	hash_set(&vm->pragma, "localsys.cmd", strdup("cw localsys"));

	/* default properties */
	hash_set(&vm->props, "version", CLOCKWORK_VERSION);
	hash_set(&vm->props, "runtime", CLOCKWORK_RUNTIME);

	list_init(&vm->acl);

	vm->stderr = stderr;
	s_push(vm, &vm->tstack, 0);

	vm->code = code;
	vm->codesize = len;
	return 0;
}

int vm_args(vm_t *vm, int argc, char **argv)
{
	assert(vm);
	int i;
	for (i = argc - 1; i > 0; i--) {
		size_t n = strlen(argv[i]) + 1;
		heap_t *h = vm_heap_alloc(vm, n);
		memcpy(h->data, argv[i], n);
		s_push(vm, &vm->dstack, h->addr);
	}
	s_push(vm, &vm->dstack, argc - 1);
	return 0;
}

#define B_ERR(...) do { \
	fprintf(vm->stderr, "pendulum bytecode error: "); \
	fprintf(vm->stderr, __VA_ARGS__); \
	fprintf(vm->stderr, "\n"); \
	return 1; \
} while (0)

#define ARG0(s) do { if ( f1 ||  f2) B_ERR(s " takes no operands");            } while (0)
#define ARG1(s) do { if (!f1 ||  f2) B_ERR(s " requires exactly one operand"); } while (0)
#define ARG2(s) do { if (!f1 || !f2) B_ERR(s " requires two operands");        } while (0)
#define REGISTER1(s) do { if (!is_register(f1)) B_ERR(s " must be given a register as its first operand"); \
	if (oper1 > NREGS) B_ERR("register index %i out of bounds", oper1); } while (0)
#define REGISTER2(s) do { if (!is_register(f2)) B_ERR(s " must be given a register as its second operand"); \
	if (oper2 > NREGS) B_ERR("register index %i out of bounds", oper2); } while (0)

int vm_exec(vm_t *vm)
{
	byte_t op, f1, f2;
	dword_t oper1, oper2;
	char *s;
	const char *v;
	acl_t *acl;

	runner_t runner;
	char execline[8192];

	vm->pc = 0;

	while (!vm->abort) {
		oper1 = oper2 = 0;
		op = vm->code[vm->pc++];
		f1 = HI_NYBLE(vm->code[vm->pc]);
		f2 = LO_NYBLE(vm->code[vm->pc]);

		if (vm->trace)
			fprintf(vm->stderr, "+%s [%02x]", OPCODES[op], vm->code[vm->pc]);
		vm->pc++;

		if (f2 && !f1)
			B_ERR("corrupt operands mask detected; f1=%02x, f2=%02x", f1, f2);

		if (f1) {
			oper1 = DWORD(vm->code[vm->pc + 0],
			              vm->code[vm->pc + 1],
			              vm->code[vm->pc + 2],
			              vm->code[vm->pc + 3]);
			vm->pc += 4;
			if (vm->trace)
				fprintf(vm->stderr, " %08x", oper1);
		}

		if (f2) {
			oper2 = DWORD(vm->code[vm->pc + 0],
			              vm->code[vm->pc + 1],
			              vm->code[vm->pc + 2],
			              vm->code[vm->pc + 3]);
			vm->pc += 4;
			if (vm->trace)
				fprintf(vm->stderr, " %08x", oper2);
		}
		if (vm->trace)
			fprintf(vm->stderr, "\n");

		switch (op) {
		case OP_NOOP:
			break;

		case OP_PUSH:
			ARG1("push");
			REGISTER1("push");

			s_push(vm, &vm->dstack, vm->r[oper1]);
			break;

		case OP_POP:
			ARG1("pop");
			REGISTER1("pop");

			vm->r[oper1] = s_pop(vm, &vm->dstack);
			break;

		case OP_SET:
			ARG2("set");
			REGISTER1("set");

			vm->r[oper1] = s_val(vm, f2, oper2);
			break;

		case OP_SWAP:
			ARG2("swap");
			REGISTER1("swap");
			REGISTER2("swap");

			if (oper1 == oper2)
				B_ERR("swap requires distinct registers");

			vm->r[oper1] ^= vm->r[oper2];
			vm->r[oper2] ^= vm->r[oper1];
			vm->r[oper1] ^= vm->r[oper2];
			break;

		case OP_ACC:
			ARG1("acc");
			REGISTER1("acc");

			vm->r[oper1] = vm->acc;
			break;

		case OP_ADD:
			ARG2("add");
			REGISTER1("add");

			vm->r[oper1] += s_val(vm, f2, oper2);
			break;

		case OP_SUB:
			ARG2("sub");
			REGISTER1("sub");

			vm->r[oper1] -= s_val(vm, f2, oper2);
			break;

		case OP_MULT:
			ARG2("mult");
			REGISTER1("mult");

			vm->r[oper1] *= s_val(vm, f2, oper2);
			break;

		case OP_DIV:
			ARG2("div");
			REGISTER1("div");

			vm->r[oper1] /= s_val(vm, f2, oper2);
			break;

		case OP_MOD:
			ARG2("mod");
			REGISTER1("mod");

			vm->r[oper1] %= s_val(vm, f2, oper2);
			break;

		case OP_CALL:
			ARG1("call");
			if (!is_address(f1))
				B_ERR("call requires an address for operand 1");

			s_save_state(vm);
			s_push(vm, &vm->istack, vm->pc);
			vm->pc = oper1;
			break;

		case OP_TRY:
			ARG1("try");
			if (!is_address(f1))
				B_ERR("try requires an address for operand 1");

			s_save_state(vm);
			s_push(vm, &vm->tstack, vm->tryc);
			vm->tryc = vm->pc;
			s_push(vm, &vm->istack, vm->pc);
			vm->pc = oper1;
			break;

		case OP_RET:
			if (f1) {
				ARG1("ret");
				vm->acc = s_val(vm, f1, oper1);
			} else {
				ARG0("ret");
			}
			if (s_empty(&vm->istack))
				return 0; /* last RET == HALT */
			vm->pc = s_pop(vm, &vm->istack);
			if (vm->tryc == vm->pc)
				vm->tryc = s_pop(vm, &vm->tstack);
			s_restore_state(vm);
			break;

		case OP_BAIL:
			ARG1("bail");
			vm->acc = s_val(vm, f1, oper1);

			if (vm->tryc == 0)
				return 0; /* last BAIL == HALT */

			while (vm->pc != vm->tryc) {
				vm->pc = s_pop(vm, &vm->istack);
				s_restore_state(vm);
			}
			vm->tryc = s_pop(vm, &vm->tstack);
			break;

		case OP_EQ:
			ARG2("eq");
			vm->acc = (s_val(vm, f1, oper1) == s_val(vm, f2, oper2)) ? 0 : 1;
			break;

		case OP_GT:
			ARG2("gt");
			vm->acc = (s_val(vm, f1, oper1) >  s_val(vm, f2, oper2)) ? 0 : 1;
			break;

		case OP_GTE:
			ARG2("gte");
			vm->acc = (s_val(vm, f1, oper1) >= s_val(vm, f2, oper2)) ? 0 : 1;
			break;

		case OP_LT:
			ARG2("lt");
			vm->acc = (s_val(vm, f1, oper1) <  s_val(vm, f2, oper2)) ? 0 : 1;
			break;

		case OP_LTE:
			ARG2("lte");
			vm->acc = (s_val(vm, f1, oper1) <= s_val(vm, f2, oper2)) ? 0 : 1;
			break;

		case OP_STREQ:
			ARG2("streq");
			vm->acc = strcmp(s_str(vm, f1, oper1), s_str(vm, f2, oper2)) == 0 ? 0 : 1;
			break;

		case OP_JMP:
			ARG1("jmp");
			vm->pc = oper1;
			break;

		case OP_JZ:
			ARG1("jz");
			if (vm->acc == 0) vm->pc = s_val(vm, f1, oper1);
			break;

		case OP_JNZ:
			ARG1("jnz");
			if (vm->acc != 0) vm->pc = s_val(vm, f1, oper1);
			break;

		case OP_STRING:
			ARG2("str");
			REGISTER2("str");

			vm->r[oper2] = vm_sprintf(vm, s_str(vm, f1, oper1));
			break;

		case OP_PRINT:
			ARG1("print");
			vm_fprintf(vm, stdout, s_str(vm, f1, oper1));
			break;

		case OP_ERROR:
			ARG1("error");
			vm_fprintf(vm, vm->stderr, s_str(vm, f1, oper1));
			fprintf(vm->stderr, "\n");
			break;

		case OP_PERROR:
			ARG1("perror");
			vm_fprintf(vm, vm->stderr, s_str(vm, f1, oper1));
			fprintf(vm->stderr, ": (%i) %s\n", errno, strerror(errno));
			break;

		case OP_SYSLOG:
			ARG2("syslog");
			s = _sprintf(vm, s_str(vm, f2, oper2));
			logger(log_level_number(s_str(vm, f1, oper1)), s); free(s);
			break;

		case OP_FLAG:
			ARG1("flag");
			hash_set(&vm->flags, s_str(vm, f1, oper1), "Y");
			vm->acc = 0;
			break;

		case OP_UNFLAG:
			ARG1("unflag");
			hash_set(&vm->flags, s_str(vm, f1, oper1), NULL);
			vm->acc = 0;
			break;

		case OP_FLAGGED_P:
			ARG1("flagged?");
			vm->acc = hash_get(&vm->flags, s_str(vm, f1, oper1)) ? 0 : 1;
			break;

		case OP_FS_STAT:
			ARG1("fs.stat");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			break;

		case OP_FS_FILE_P:
			ARG1("fs.file?");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->acc = S_ISREG(vm->aux.stat.st_mode) ? 0 : 1;
			break;

		case OP_FS_SYMLINK_P:
			ARG1("fs.symlink?");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->acc = S_ISLNK(vm->aux.stat.st_mode) ? 0 : 1;
			break;

		case OP_FS_DIR_P:
			ARG1("fs.dir?");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->acc = S_ISDIR(vm->aux.stat.st_mode) ? 0 : 1;
			break;

		case OP_FS_CHARDEV_P:
			ARG1("fs.chardev?");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->acc = S_ISCHR(vm->aux.stat.st_mode) ? 0 : 1;
			break;

		case OP_FS_BLOCKDEV_P:
			ARG1("fs.blockdev?");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->acc = S_ISBLK(vm->aux.stat.st_mode) ? 0 : 1;
			break;

		case OP_FS_FIFO_P:
			ARG1("fs.fifo?");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->acc = S_ISFIFO(vm->aux.stat.st_mode) ? 0 : 1;
			break;

		case OP_FS_SOCKET_P:
			ARG1("fs.socket?");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->acc = S_ISSOCK(vm->aux.stat.st_mode) ? 0 : 1;
			break;

		case OP_FS_TOUCH:
			ARG1("fs.touch");
			vm->acc = close(open(s_str(vm, f1, oper1), O_CREAT, 0777));
			break;

		case OP_FS_UNLINK:
			ARG1("fs.unlink");
			vm->acc = unlink(s_str(vm, f1, oper1));
			break;

		case OP_FS_RENAME:
			ARG2("fs.rename");
			vm->acc = rename(s_str(vm, f1, oper1), s_str(vm, f2, oper2));
			break;

		case OP_FS_CHOWN:
			ARG2("fs.chown");
			vm->acc = lchown(s_str(vm, f1, oper1), s_val(vm, f2, oper2), -1);
			break;

		case OP_FS_CHGRP:
			ARG2("fs.chgrp");
			vm->acc = lchown(s_str(vm, f1, oper1), -1, s_val(vm, f2, oper2));
			break;

		case OP_FS_CHMOD:
			ARG2("fs.chmod");
			vm->acc = chmod(s_str(vm, f1, oper1), s_val(vm, f2, oper2) & 07777);
			break;

		case OP_FS_SHA1:
			ARG2("fs.sha1");
			REGISTER2("fs.sha1");
			vm->acc = sha1_file(s_str(vm, f1, oper1), &vm->aux.sha1);
			vm->r[oper2] = vm_heap_strdup(vm, vm->aux.sha1.hex);
			break;

		case OP_FS_DEV:
			ARG2("fs.dev");
			REGISTER2("fs.dev");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_dev;
			break;

		case OP_FS_INODE:
			ARG2("fs.inode");
			REGISTER2("fs.inode");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_ino;
			break;

		case OP_FS_MODE:
			ARG2("fs.mode");
			REGISTER2("fs.mode");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_mode & 07777;
			break;

		case OP_FS_NLINK:
			ARG2("fs.nlink");
			REGISTER2("fs.nlink");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_nlink;
			break;

		case OP_FS_UID:
			ARG2("fs.uid");
			REGISTER2("fs.uid");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_uid;
			break;

		case OP_FS_GID:
			ARG2("fs.gid");
			REGISTER2("fs.gid");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_gid;
			break;

		case OP_FS_MAJOR:
			ARG2("fs.major");
			REGISTER2("fs.major");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = major(vm->aux.stat.st_rdev);
			break;

		case OP_FS_MINOR:
			ARG2("fs.minor");
			REGISTER2("fs.minor");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = minor(vm->aux.stat.st_rdev);
			break;

		case OP_FS_SIZE:
			ARG2("fs.size");
			REGISTER2("fs.size");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_size;
			break;

		case OP_FS_ATIME:
			ARG2("fs.atime");
			REGISTER2("fs.atime");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_atime;
			break;

		case OP_FS_MTIME:
			ARG2("fs.mtime");
			REGISTER2("fs.mtime");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_mtime;
			break;

		case OP_FS_CTIME:
			ARG2("fs.ctime");
			REGISTER2("fs.ctime");

			vm->acc = lstat(s_str(vm, f1, oper1), &vm->aux.stat);
			if (vm->acc == 0) vm->r[oper2] = vm->aux.stat.st_ctime;
			break;

		case OP_LOCALSYS:
			ARG2("localsys");
			REGISTER2("localsys");
			runner.in  = NULL;
			runner.out = tmpfile();
			runner.err = tmpfile();
			runner.uid = geteuid();
			runner.gid = getegid();

			s = string("%s %s",
				hash_get(&vm->pragma, "localsys.cmd"),
				s_str(vm, f1, oper1));
			vm->acc = run2(&runner, "/bin/sh", "-c", s, NULL); free(s);

			if (fgets(execline, sizeof(execline), runner.out)) {
				s = strchr(execline, '\n'); if (s) *s = '\0';
				vm->r[oper2] = vm_heap_strdup(vm, execline);
			}
			fclose(runner.out); runner.out = NULL;
			fclose(runner.err); runner.err = NULL;
			break;

		case OP_EXEC:
			ARG2("exec");
			REGISTER2("exec");
			runner.in  = NULL;
			runner.out = tmpfile();
			runner.err = tmpfile();
			runner.uid = geteuid();
			runner.gid = getegid();
			vm->acc = run2(&runner, "/bin/sh", "-c", s_str(vm, f1, oper1), NULL);
			if (fgets(execline, sizeof(execline), runner.out)) {
				s = strchr(execline, '\n'); if (s) *s = '\0';
				vm->r[oper2] = vm_heap_strdup(vm, execline);
			}
			fclose(runner.out); runner.out = NULL;
			fclose(runner.err); runner.err = NULL;
			break;

		case OP_AUTHDB_OPEN:
			ARG0("authdb.open");

			authdb_close(vm->aux.authdb);
			vm->aux.authdb = authdb_read(hash_get(&vm->pragma, "authdb.root"), AUTHDB_ALL);
			vm->acc = vm->aux.authdb != NULL ? 0 : 1;
			break;

		case OP_AUTHDB_SAVE:
			ARG0("authdb.save");
			vm->acc = authdb_write(vm->aux.authdb);
			break;

		case OP_AUTHDB_CLOSE:
			ARG0("authdb.close");
			authdb_close(vm->aux.authdb);
			vm->aux.authdb = NULL;
			vm->acc = 0;
			break;

		case OP_AUTHDB_NEXTUID:
			ARG2("authdb.nextuid");
			REGISTER2("authdb.nextuid");
			vm->acc = authdb_nextuid(vm->aux.authdb, s_val(vm, f1, oper1));
			if (vm->acc < 65536) {
				vm->r[oper2] = vm->acc;
				vm->acc = 0;
			}
			break;

		case OP_AUTHDB_NEXTGID:
			ARG2("authdb.nextgid");
			REGISTER2("authdb.nextuid");
			vm->acc = authdb_nextgid(vm->aux.authdb, s_val(vm, f1, oper1));
			if (vm->acc < 65536) {
				vm->r[oper2] = vm->acc;
				vm->acc = 0;
			}
			break;

		case OP_USER_FIND:
			ARG1("user.find");
			vm->aux.user = user_find(vm->aux.authdb, s_str(vm, f1, oper1), -1);
			vm->acc = vm->aux.user ? 0 : 1;
			break;

		case OP_USER_GET:
			ARG2("user.get");
			REGISTER2("user.get");
			if (!vm->aux.user) {
				vm->acc = 1;
				break;
			}

			vm->acc = 0;
			v = s_str(vm, f1, oper1);

			if (strcmp(v, "uid") == 0) {
				vm->r[oper2] = vm->aux.user->uid;

			} else if (strcmp(v, "gid") == 0) {
				vm->r[oper2] = vm->aux.user->gid;

			} else if (strcmp(v, "username") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.user->name);

			} else if (strcmp(v, "comment") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.user->comment);

			} else if (strcmp(v, "home") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.user->home);

			} else if (strcmp(v, "shell") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.user->shell);

			} else if (strcmp(v, "password") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.user->clear_pass);

			} else if (strcmp(v, "pwhash") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.user->crypt_pass);

			} else if (strcmp(v, "changed") == 0) {
				vm->r[oper2] = vm->aux.user->creds.last_changed;

			} else if (strcmp(v, "pwmin") == 0) {
				vm->r[oper2] = vm->aux.user->creds.min_days;

			} else if (strcmp(v, "pwmax") == 0) {
				vm->r[oper2] = vm->aux.user->creds.max_days;

			} else if (strcmp(v, "pwwarn") == 0) {
				vm->r[oper2] = vm->aux.user->creds.warn_days;

			} else if (strcmp(v, "inact") == 0) {
				vm->r[oper2] = vm->aux.user->creds.grace_period;

			} else if (strcmp(v, "expiry") == 0) {
				vm->r[oper2] = vm->aux.user->creds.expiration;

			} else {
				vm->acc = 1;
			}
			break;

		case OP_USER_SET:
			ARG2("user.set");
			if (!vm->aux.user) {
				vm->acc = 1;
				break;
			}

			vm->acc = 0;
			v = s_str(vm, f1, oper1);

			if (strcmp(v, "uid") == 0) {
				vm->aux.user->uid = s_val(vm, f2, oper2);

			} else if (strcmp(v, "gid") == 0) {
				vm->aux.user->gid = s_val(vm, f2, oper2);

			} else if (strcmp(v, "username") == 0) {
				free(vm->aux.user->name);
				vm->aux.user->name = strdup(s_str(vm, f2, oper2));

			} else if (strcmp(v, "comment") == 0) {
				free(vm->aux.user->comment);
				vm->aux.user->comment = strdup(s_str(vm, f2, oper2));

			} else if (strcmp(v, "home") == 0) {
				free(vm->aux.user->home);
				vm->aux.user->home = strdup(s_str(vm, f2, oper2));

			} else if (strcmp(v, "shell") == 0) {
				free(vm->aux.user->shell);
				vm->aux.user->shell = strdup(s_str(vm, f2, oper2));

			} else if (strcmp(v, "password") == 0) {
				free(vm->aux.user->clear_pass);
				vm->aux.user->clear_pass = strdup(s_str(vm, f2, oper2));

			} else if (strcmp(v, "pwhash") == 0) {
				free(vm->aux.user->crypt_pass);
				vm->aux.user->crypt_pass = strdup(s_str(vm, f2, oper2));

			} else if (strcmp(v, "changed") == 0) {
				vm->aux.user->creds.last_changed = s_val(vm, f2, oper2);

			} else if (strcmp(v, "pwmin") == 0) {
				vm->aux.user->creds.min_days = s_val(vm, f2, oper2);

			} else if (strcmp(v, "pwmax") == 0) {
				vm->aux.user->creds.max_days = s_val(vm, f2, oper2);

			} else if (strcmp(v, "pwwarn") == 0) {
				vm->aux.user->creds.warn_days = s_val(vm, f2, oper2);

			} else if (strcmp(v, "inact") == 0) {
				vm->aux.user->creds.grace_period = s_val(vm, f2, oper2);

			} else if (strcmp(v, "expiry") == 0) {
				vm->aux.user->creds.expiration = s_val(vm, f2, oper2);

			} else {
				vm->acc = 1;
			}
			break;

		case OP_USER_NEW:
			ARG0("user.new");
			vm->aux.user = user_add(vm->aux.authdb);
			vm->acc = vm->aux.user ? 0 : 1;
			break;

		case OP_USER_DELETE:
			ARG0("user.delete");
			if (!vm->aux.user) {
				vm->acc = 1;
				break;
			}
			user_remove(vm->aux.user);
			vm->acc = 0;
			break;

		case OP_GROUP_FIND:
			ARG1("group.find");
			vm->aux.group = group_find(vm->aux.authdb, s_str(vm, f1, oper1), -1);
			vm->acc = vm->aux.group ? 0 : 1;
			break;

		case OP_GROUP_GET:
			ARG2("group.get");
			REGISTER2("group.get");
			if (!vm->aux.group) {
				vm->acc = 1;
				break;
			}

			vm->acc = 0;
			v = s_str(vm, f1, oper1);

			if (strcmp(v, "gid") == 0) {
				vm->r[oper2] = vm->aux.group->gid;

			} else if (strcmp(v, "name") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.group->name);

			} else if (strcmp(v, "password") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.group->clear_pass);

			} else if (strcmp(v, "pwhash") == 0) {
				vm->r[oper2] = vm_heap_strdup(vm, vm->aux.group->crypt_pass);

			} else {
				vm->acc = 1;
			}
			break;

		case OP_GROUP_SET:
			ARG2("group.get");
			if (!vm->aux.group) {
				vm->acc = 1;
				break;
			}

			vm->acc = 0;
			v = s_str(vm, f1, oper1);

			if (strcmp(v, "gid") == 0) {
				vm->aux.group->gid = s_val(vm, f2, oper2);

			} else if (strcmp(v, "name") == 0) {
				free(vm->aux.group->name);
				vm->aux.group->name = strdup(s_str(vm, f2, oper2));

			} else if (strcmp(v, "password") == 0) {
				free(vm->aux.group->clear_pass);
				vm->aux.group->clear_pass = strdup(s_str(vm, f2, oper2));

			} else if (strcmp(v, "pwhash") == 0) {
				free(vm->aux.group->crypt_pass);
				vm->aux.group->crypt_pass = strdup(s_str(vm, f2, oper2));

			} else {
				vm->acc = 1;
			}
			break;

		case OP_GROUP_NEW:
			ARG0("group.new");
			vm->aux.group = group_add(vm->aux.authdb);
			vm->acc = vm->aux.group ? 0 : 1;
			break;

		case OP_GROUP_DELETE:
			ARG0("group.delete");
			if (!vm->aux.group) {
				vm->acc = 1;
				break;
			}
			group_remove(vm->aux.group);
			vm->acc = 0;
			break;

		case OP_AUGEAS_INIT:
			ARG0("augeas.init");
			vm->aux.augeas = aug_init(
				hash_get(&vm->pragma, "augeas.root"),
				hash_get(&vm->pragma, "augeas.libs"),
				AUG_NO_STDINC|AUG_NO_LOAD|AUG_NO_MODL_AUTOLOAD);

			if (aug_set(vm->aux.augeas, "/augeas/load/Hosts/lens", "Hosts.lns") < 0
			 || aug_set(vm->aux.augeas, "/augeas/load/Hosts/incl", "/etc/hosts") < 0
			 || aug_load(vm->aux.augeas) != 0) {
				vm->acc = 1;
			} else {
				vm->acc = 0;
			}
			break;

		case OP_AUGEAS_DONE:
			ARG0("augeas.done");
			aug_close(vm->aux.augeas);
			vm->aux.augeas = NULL;
			vm->acc = 0;
			break;

		case OP_AUGEAS_PERROR:
			ARG1("augeas.perror");
			vm_fprintf(vm, vm->stderr, s_str(vm, f1, oper1));
			s_aug_perror(vm->stderr, vm->aux.augeas);
			break;

		case OP_AUGEAS_WRITE:
			ARG0("augeas.write");
			vm->acc = aug_save(vm->aux.augeas);
			break;

		case OP_AUGEAS_SET:
			ARG2("augeas.set");
			vm->acc = aug_set(vm->aux.augeas, s_str(vm, f1, oper1), s_str(vm, f2, oper2));
			break;

		case OP_AUGEAS_GET:
			ARG2("augeas.get");
			REGISTER2("augeas.get");
			if (aug_get(vm->aux.augeas, s_str(vm, f1, oper1), &v) == 1 && v) {
				vm->r[oper2] = vm_heap_strdup(vm, v);
				vm->acc = 0;
			} else {
				vm->acc = 1;
			}
			break;

		case OP_AUGEAS_FIND:
			ARG2("augeas.find");
			REGISTER2("augeas.find");
			s = s_aug_find(vm->aux.augeas, s_str(vm, f1, oper1));
			if (s) {
				vm->acc = 0;
				vm->r[oper2] = vm_heap_strdup(vm, s); free(s);
			} else {
				vm->acc = 1;
			}
			break;

		case OP_AUGEAS_REMOVE:
			ARG1("augeas.remove");
			vm->acc = aug_rm(vm->aux.augeas, s_str(vm, f1, oper1)) > 1 ? 0 : 1;
			break;

		case OP_ENV_GET:
			ARG2("env.get");
			REGISTER2("env.get");
			s = getenv(s_str(vm, f1, oper1));
			if (s) {
				vm->r[oper2] = vm_heap_strdup(vm, s);
				vm->acc = 0;
			} else {
				vm->acc = 1;
			}
			break;

		case OP_ENV_SET:
			ARG2("env.set");
			vm->acc = setenv(s_str(vm, f1, oper1), s_str(vm, f2, oper2), 1);
			break;

		case OP_ENV_UNSET:
			ARG1("env.unset");
			vm->acc = unsetenv(s_str(vm, f1, oper1));
			break;

		case OP_HALT:
			ARG0("halt");
			return 0;

		case OP_DUMP:
			ARG0("dump");
			dump(stdout, vm);
			break;

		case OP_PRAGMA:
			ARG2("pragma");
			v = s_str(vm, f1, oper1);

			if (strcmp(v, "test") == 0) {
				vm->stderr = strcmp(s_str(vm, f2, oper2), "on") == 0 ? stdout : stderr;

			} else if (strcmp(v, "trace") == 0) {
				vm->trace = strcmp(s_str(vm, f2, oper2), "on") == 0;

			} else {
				/* set the value in the pragma hash */
				free(hash_set(&vm->pragma, v, strdup(s_str(vm, f2, oper2))));
			}
			break;

		case OP_PROPERTY:
			ARG2("property");
			REGISTER2("property");
			v = hash_get(&vm->props, s_str(vm, f1, oper1));
			if (v) {
				vm->r[oper2] = vm_heap_strdup(vm, v);
				vm->acc = 0;
			} else {
				vm->acc = 1;
			}
			break;

		case OP_ACL:
			ARG1("acl");
			acl = acl_parse(s_str(vm, f1, oper1));
			list_push(&vm->acl, &acl->l);
			vm->acc = 0;
			break;

		case OP_SHOW_ACLS:
			ARG0("show.acls");
			for_each_object(acl, &vm->acl, l) {
				fprintf(stdout, "%s\n", s = acl_string(acl));
				free(s);
			}
			break;

		case OP_SHOW_ACL:
			ARG1("show.acls");
			v = s_str(vm, f1, oper1);
			for_each_object(acl, &vm->acl, l) {
				if (!acl_match(acl, v, NULL)) continue;
				fprintf(stdout, "%s\n", s = acl_string(acl));
				free(s);
			}
			break;

		default:
			B_ERR("unknown operand %02x", op);
			return -1;
		}
	}

	return 1;
}

int vm_done(vm_t *vm)
{
	heap_t *h, *tmp;
	for_each_object_safe(h, tmp, &vm->heap, l) {
		list_delete(&h->l);
		free(h->data);
		free(h);
	}

	hash_done(&vm->pragma, 1);
	hash_done(&vm->flags,  0);
	return 0;
}
