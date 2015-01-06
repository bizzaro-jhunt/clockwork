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

#include "opcodes.h"

static int s_empty(stack_t *st)
{
	return st->top == 0;
}

static int s_push(stack_t *st, dword_t value)
{
	assert(st);
	if (st->top == 254) {
		fprintf(stderr, "stack overflow!\n");
		abort();
	}

	st->val[st->top++] = value;
	return 0;
}

static dword_t s_pop(stack_t *st)
{
	assert(st);
	if (s_empty(st)) {
		fprintf(stderr, "stack underflow!\n");
		abort();
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
		s_push(&vm->dstack, vm->r[i]);
}

static void s_restore_state(vm_t *vm)
{
	int i;
	for (i = NREGS - 1; i >= 0; i--)
		vm->r[i] = s_pop(&vm->dstack);
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
	h->addr   = vm->heaptop++ | HEAP_ADDRMASK;
	h->size   = n;
	if (n)
		h->data = calloc(n, sizeof(byte_t));
	list_push(&vm->heap, &h->l);
	return h;
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
				*++b = '\0';
				a = ++b;
				*s++ = '%';
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
		s_push(&vm->dstack, h->addr);
	}
	s_push(&vm->dstack, argc - 1);
	return 0;
}

#define B_ERR(...) do { \
	fprintf(stderr, "regm bytecode error: "); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	return -1; \
} while (0)

#define ARG0(s) do { if ( f1 ||  f2) B_ERR(s " takes no operands");            } while (0)
#define ARG1(s) do { if (!f1 ||  f2) B_ERR(s " requires exactly one operand"); } while (0)
#define ARG2(s) do { if (!f1 || !f2) B_ERR(s " requires two operands");        } while (0)
#define NEED_STAT(s) do { if (!vm->stat.st_ino) B_ERR(s " called without a prior fs.stat\n"); } while (0)

int vm_exec(vm_t *vm)
{
	byte_t op, f1, f2;
	dword_t oper1, oper2;
	vm->pc = 0;

	for (;;) {
		oper1 = oper2 = 0;
		op = vm->code[vm->pc++];
		f1 = HI_NYBLE(vm->code[vm->pc]);
		f2 = LO_NYBLE(vm->code[vm->pc]);
		vm->pc++;

		if (f2 && !f1)
			B_ERR("corrupt operands mask detected; f1=%02x, f2=%02x", f1, f2);

		if (f1) {
			oper1 = DWORD(vm->code[vm->pc + 0],
			              vm->code[vm->pc + 1],
			              vm->code[vm->pc + 2],
			              vm->code[vm->pc + 3]);
			vm->pc += 4;
		}

		if (f2) {
			oper2 = DWORD(vm->code[vm->pc + 0],
			              vm->code[vm->pc + 1],
			              vm->code[vm->pc + 2],
			              vm->code[vm->pc + 3]);
			vm->pc += 4;
		}

		switch (op) {
		case NOOP:
			break;

		case PUSH:
			ARG1("push");
			if (!is_register(f1))
				B_ERR("push requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			s_push(&vm->dstack, vm->r[oper1]);
			break;

		case POP:
			ARG1("pop");
			if (!is_register(f1))
				B_ERR("pop requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			vm->r[oper1] = s_pop(&vm->dstack);
			break;

		case SET:
			ARG2("set");
			if (!is_register(f1))
				B_ERR("set requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			vm->r[oper1] = s_val(vm, f2, oper2);
			break;

		case SWAP:
			ARG2("swap");
			if (!is_register(f1))
				B_ERR("swap requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			if (!is_register(f2))
				B_ERR("swap requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			if (oper1 == oper2)
				B_ERR("swap requires distinct registers for operands");

			vm->r[oper1] ^= vm->r[oper2];
			vm->r[oper2] ^= vm->r[oper1];
			vm->r[oper1] ^= vm->r[oper2];
			break;

		case ADD:
			ARG2("add");
			if (!is_register(f1))
				B_ERR("add requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			vm->r[oper1] += s_val(vm, f2, oper2);
			break;

		case SUB:
			ARG2("sub");
			if (!is_register(f1))
				B_ERR("sub requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			vm->r[oper1] -= s_val(vm, f2, oper2);
			break;

		case MULT:
			ARG2("mult");
			if (!is_register(f1))
				B_ERR("mult requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			vm->r[oper1] *= s_val(vm, f2, oper2);
			break;

		case DIV:
			ARG2("div");
			if (!is_register(f1))
				B_ERR("div requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			vm->r[oper1] /= s_val(vm, f2, oper2);
			break;

		case MOD:
			ARG2("mod");
			if (!is_register(f1))
				B_ERR("mod requires a register index for operand 1");
			if (oper1 > NREGS)
				B_ERR("register %08x is out of bounds", oper1);

			vm->r[oper1] %= s_val(vm, f2, oper2);
			break;

		case CALL:
			ARG1("call");
			if (!is_address(f1))
				B_ERR("call requires an address for operand 1");

			s_save_state(vm);
			s_push(&vm->istack, vm->pc);
			vm->pc = oper1;
			break;

		case RET:
			if (f1) {
				ARG1("ret");
				vm->acc = s_val(vm, f1, oper1);
			} else {
				ARG0("ret");
			}
			if (s_empty(&vm->istack))
				return 0; /* last RET == HALT */
			vm->pc = s_pop(&vm->istack);
			s_restore_state(vm);
			break;

		case CMP:
			ARG2("cmp");
			vm->acc = s_val(vm, f1, oper1) - s_val(vm, f2, oper2);
			break;

		case STRCMP:
			ARG2("strcmp");
			vm->acc = strcmp(s_str(vm, f1, oper1), s_str(vm, f1, oper2));
			break;

		case JMP:
			ARG1("jmp");
			vm->pc = oper1;
			break;

		case JZ:
			ARG1("jz");
			if (vm->acc == 0) vm->pc = s_val(vm, f1, oper1);
			break;

		case JNZ:
			ARG1("jnz");
			if (vm->acc != 0) vm->pc = s_val(vm, f1, oper1);
			break;

		case JE:
			ARG2("je");
			if (vm->acc == s_val(vm, f1, oper1))
				vm->pc = s_val(vm, f2, oper2);
			break;

		case JNE:
			ARG2("jne");
			if (vm->acc != s_val(vm, f1, oper1))
				vm->pc = s_val(vm, f2, oper2);
			break;

		case JGT:
			ARG2("jgt");
			if (vm->acc > s_val(vm, f1, oper1))
				vm->pc = s_val(vm, f2, oper2);
			break;

		case JGTE:
			ARG2("jgte");
			if (vm->acc >= s_val(vm, f1, oper1))
				vm->pc = s_val(vm, f2, oper2);
			break;

		case JLT:
			ARG2("jlt");
			if (vm->acc < s_val(vm, f1, oper1))
				vm->pc = s_val(vm, f2, oper2);
			break;

		case JLTE:
			ARG2("jlte");
			if (vm->acc <= s_val(vm, f1, oper1))
				vm->pc = s_val(vm, f2, oper2);
			break;

		case STR:
			ARG2("str");
			if (!is_register(f2))
				B_ERR("str requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm_sprintf(vm, s_str(vm, f1, oper1));
			break;

		case ECHO:
			ARG1("echo");
			vm_fprintf(vm, stdout, s_str(vm, f1, oper1));
			break;

		case ERR:
			ARG1("err");
			vm_fprintf(vm, stderr, s_str(vm, f1, oper1));
			fprintf(stderr, "\n");
			break;

		case PERROR:
			ARG1("perror");
			vm_fprintf(vm, stderr, s_str(vm, f1, oper1));
			fprintf(stderr, ": (%i) %s\n", errno, strerror(errno));
			break;

		case BAIL:
			ARG0("bail");
			printf("bail\n"); /* FIXME: not implemented */
			break;

		case MARK:
			ARG0("mark");
			printf("mark\n"); /* FIXME: not implemented */
			break;

		case FS_STAT:
			ARG1("fs.stat");
			vm->acc = lstat(s_str(vm, f1, oper1), &vm->stat);
			break;

		case FS_FILE_P:
			ARG1("fs.file?");
			NEED_STAT("fs.file?");
			vm->acc = S_ISREG(vm->stat.st_mode) ? 0 : 1;
			break;

		case FS_SYMLINK_P:
			ARG1("fs.symlink?");
			NEED_STAT("fs.symlink?");
			vm->acc = S_ISLNK(vm->stat.st_mode) ? 0 : 1;
			break;

		case FS_DIR_P:
			ARG1("fs.dir?");
			NEED_STAT("fs.dir?");
			vm->acc = S_ISDIR(vm->stat.st_mode) ? 0 : 1;
			break;

		case FS_CHARDEV_P:
			ARG1("fs.chardev?");
			NEED_STAT("fs.chardev?");
			vm->acc = S_ISCHR(vm->stat.st_mode) ? 0 : 1;
			break;

		case FS_BLOCKDEV_P:
			ARG1("fs.blockdev?");
			NEED_STAT("fs.blockdev?");
			vm->acc = S_ISBLK(vm->stat.st_mode) ? 0 : 1;
			break;

		case FS_FIFO_P:
			ARG1("fs.fifo?");
			NEED_STAT("fs.fifo?");
			vm->acc = S_ISFIFO(vm->stat.st_mode) ? 0 : 1;
			break;

		case FS_SOCKET_P:
			ARG1("fs.socket?");
			NEED_STAT("fs.socket?");
			vm->acc = S_ISSOCK(vm->stat.st_mode) ? 0 : 1;
			break;

		case FS_TOUCH:
			ARG1("fs.touch");
			vm->acc = close(open(s_str(vm, f1, oper1), O_CREAT, 0777));
			break;

		case FS_UNLINK:
			ARG1("fs.unlink");
			vm->acc = unlink(s_str(vm, f1, oper1));
			break;

		case FS_RENAME:
			ARG2("fs.rename");
			vm->acc = rename(s_str(vm, f1, oper1), s_str(vm, f2, oper2));
			break;

		case FS_CHOWN:
			ARG2("fs.chown");
			vm->acc = lchown(s_str(vm, f1, oper1), s_val(vm, f2, oper2), -1);
			break;

		case FS_CHGRP:
			ARG2("fs.chgrp");
			vm->acc = lchown(s_str(vm, f1, oper1), -1, s_val(vm, f2, oper2));
			break;

		case FS_CHMOD:
			ARG2("fs.chmod");
			vm->acc = chmod(s_str(vm, f1, oper1), s_val(vm, f2, oper2) & 0x7777);
			break;

		case FS_SHA1:
			printf("fs.sha1\n"); /* FIXME: not implemented */
			break;

		case FS_DEV:
			ARG2("fs.dev");
			NEED_STAT("fs.dev");
			if (!is_register(f2))
				B_ERR("fs.dev requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_dev;
			break;

		case FS_INODE:
			ARG2("fs.inode");
			NEED_STAT("fs.inode");
			if (!is_register(f2))
				B_ERR("fs.inode requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_ino;
			break;

		case FS_MODE:
			ARG2("fs.mode");
			NEED_STAT("fs.mode");
			if (!is_register(f2))
				B_ERR("fs.mode requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_mode;
			break;

		case FS_NLINK:
			ARG2("fs.nlink");
			NEED_STAT("fs.nlink");
			if (!is_register(f2))
				B_ERR("fs.nlink requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_nlink;
			break;

		case FS_UID:
			ARG2("fs.uid");
			NEED_STAT("fs.uid");
			if (!is_register(f2))
				B_ERR("fs.uid requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_uid;
			break;

		case FS_GID:
			ARG2("fs.gid");
			NEED_STAT("fs.gid");
			if (!is_register(f2))
				B_ERR("fs.gid requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_gid;
			break;

		case FS_MAJOR:
			ARG2("fs.major");
			NEED_STAT("fs.major");
			if (!is_register(f2))
				B_ERR("fs.major requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = major(vm->stat.st_rdev);
			break;

		case FS_MINOR:
			ARG2("fs.minor");
			NEED_STAT("fs.minor");
			if (!is_register(f2))
				B_ERR("fs.minor requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = minor(vm->stat.st_rdev);
			break;

		case FS_SIZE:
			ARG2("fs.size");
			NEED_STAT("fs.size");
			if (!is_register(f2))
				B_ERR("fs.size requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_blocks * vm->stat.st_blksize;
			break;

		case FS_ATIME:
			ARG2("fs.atime");
			NEED_STAT("fs.atime");
			if (!is_register(f2))
				B_ERR("fs.atime requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_atime;
			break;

		case FS_MTIME:
			ARG2("fs.mtime");
			NEED_STAT("fs.mtime");
			if (!is_register(f2))
				B_ERR("fs.mtime requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_mtime;
			break;

		case FS_CTIME:
			ARG2("fs.ctime");
			NEED_STAT("fs.ctime");
			if (!is_register(f2))
				B_ERR("fs.ctime requires a register index for operand 2");
			if (oper2 > NREGS)
				B_ERR("register %08x is out of bounds", oper2);

			vm->r[oper2] = vm->stat.st_ctime;
			break;

		case GETFILE:
			printf("getfile\n"); /* FIXME: not implemented */
			break;

		case EXEC:
			printf("exec\n"); /* FIXME: not implemented */
			break;

		case PASSWD_OPEN:
			printf("passwd.open\n"); /* FIXME: not implemented */
			break;

		case PASSWD_SAVE:
			printf("passwd.save\n"); /* FIXME: not implemented */
			break;

		case PASSWD_CLOSE:
			printf("passwd.close\n"); /* FIXME: not implemented */
			break;

		case PASSWD_NEXTUID:
			printf("passwd.nextuid\n"); /* FIXME: not implemented */
			break;

		case PASSWD_NEXTGID:
			printf("passwd.nextgid\n"); /* FIXME: not implemented */
			break;

		case USER_FIND:
			printf("user.find\n"); /* FIXME: not implemented */
			break;

		case USER_GET:
			printf("user.get\n"); /* FIXME: not implemented */
			break;

		case USER_SET:
			printf("user.set\n"); /* FIXME: not implemented */
			break;

		case USER_NEW:
			printf("user.new\n"); /* FIXME: not implemented */
			break;

		case USER_SAVE:
			printf("user.save\n"); /* FIXME: not implemented */
			break;

		case USER_DELETE:
			printf("user.delete\n"); /* FIXME: not implemented */
			break;

		case GROUP_FIND:
			printf("group.find\n"); /* FIXME: not implemented */
			break;

		case GROUP_GET:
			printf("group.get\n"); /* FIXME: not implemented */
			break;

		case GROUP_SET:
			printf("group.set\n"); /* FIXME: not implemented */
			break;

		case GROUP_NEW:
			printf("group.new\n"); /* FIXME: not implemented */
			break;

		case GROUP_SAVE:
			printf("group.save\n"); /* FIXME: not implemented */
			break;

		case GROUP_DELETE:
			printf("group.delete\n"); /* FIXME: not implemented */
			break;

		case AUGEAS_INIT:
			printf("augeas.init\n"); /* FIXME: not implemented */
			break;

		case AUGEAS_DONE:
			printf("augeas.done\n"); /* FIXME: not implemented */
			break;

		case AUGEAS_ERR:
			printf("augeas.err\n"); /* FIXME: not implemented */
			break;

		case AUGEAS_WRITE:
			printf("augeas.write\n"); /* FIXME: not implemented */
			break;

		case AUGEAS_SET:
			printf("augeas.set\n"); /* FIXME: not implemented */
			break;

		case AUGEAS_GET:
			printf("augeas.get\n"); /* FIXME: not implemented */
			break;

		case AUGEAS_FIND:
			printf("augeas.find\n"); /* FIXME: not implemented */
			break;

		case AUGEAS_REMOVE:
			printf("augeas.remove\n"); /* FIXME: not implemented */
			break;

		case HALT:
			ARG0("halt");
			return 0;

		case DUMP:
			ARG0("dump");
			dump(stderr, vm);
			break;

		default:
			B_ERR("unknown operand %02x", op);
			return -1;
		}
	}
}

int vm_done(vm_t *vm)
{
	heap_t *h, *tmp;
	for_each_object_safe(h, tmp, &vm->heap, l) {
		list_delete(&h->l);
		free(h->data);
		free(h);
	}
	return 0;
}
