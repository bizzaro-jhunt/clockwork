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
#include <dirent.h>

#define OPCODES_INTERPRETER
#include <augeas.h>
#include "opcodes.h"
#include "authdb.h"
#include "mesh.h"

#define T_REGISTER        0x01
#define T_LABEL           0x02
#define T_IDENTIFIER      0x03
#define T_OFFSET          0x04
#define T_NUMBER          0x05
#define T_STRING          0x06
#define T_OPCODE          0x07
#define T_FUNCTION        0x08
#define T_ACL             0x09

/************************************************************************/

static void s_eprintf(FILE *io, const char *orig)
{
	const char *p = orig;
	while (*p) {
		switch (*p) {
		case '\r': fprintf(io, "\\r"); break;
		case '\n': fprintf(io, "\\n"); break;
		case '\t': fprintf(io, "\\t"); break;
		case '\"': fprintf(io, "\\\""); break;
		default:
			if (isprint(*p)) fprintf(io, "%c", *p);
			else             fprintf(io, "\\x%02x", *p);
		}
		p++;
	}
}

static int s_empty(stack_t *st)
{
	return st->top == 0;
}

static int s_push(vm_t *vm, stack_t *st, dword_t value)
{
	assert(st);
	if (st->top == 254) {
		fprintf(vm->stderr, "stack overflow!\n");
		vm->stop = 1; vm->acc  = 1;
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
		vm->stop = 1; vm->acc  = 1;
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
	} else if (arg > 0 && arg < vm->codesize) {
		return (void *)(vm->code + arg);
	}
	return NULL;
}

static const char *s_str(vm_t *vm, byte_t type, dword_t arg)
{
	char *s;
	switch (type) {
	case TYPE_ADDRESS:  s = (char *)s_ptr(vm, arg);        break;
	case TYPE_REGISTER: s = (char *)s_ptr(vm, vm->r[arg]); break;
	default:            s = NULL;                          break;
	}

	if (!s) s = "";
	return s;
}
#define STR1(vm) s_str(vm, vm->f1, vm->oper1)
#define STR2(vm) s_str(vm, vm->f2, vm->oper2)

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
#define VAL1(vm) s_val(vm, vm->f1, vm->oper1)
#define VAL2(vm) s_val(vm, vm->f2, vm->oper2)

#define REG1(vm) vm->r[vm->oper1]
#define REG2(vm) vm->r[vm->oper2]

static void s_save_state(vm_t *vm)
{
	int i;
	for (i = 0; i < NREGS; i++)
		s_push(vm, &vm->rstack, vm->r[i]);
}

static void s_restore_state(vm_t *vm)
{
	int i;
	for (i = NREGS - 1; i >= 0; i--)
		vm->r[i] = s_pop(vm, &vm->rstack);
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
	heap_t *h = vmalloc(sizeof(heap_t));
	h->addr = vm->heaptop++ | HEAP_ADDRMASK;
	h->size = n;
	if (n)
		h->data = calloc(n, sizeof(byte_t));
	list_push(&vm->heap, &h->l);
	return h;
}

static dword_t vm_heap_string(vm_t *vm, char *s)
{
	heap_t *h = vmalloc(sizeof(heap_t));
	h->addr = vm->heaptop++ | HEAP_ADDRMASK;
	h->size = strlen(s) + 1;
	h->data = (byte_t *)s;
	list_push(&vm->heap, &h->l);
	return h->addr;
}

static dword_t vm_heap_strdup(vm_t *vm, const char *s)
{
	return vm_heap_string(vm, strdup(s));
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
		fprintf(io, "          '----------'\n");
	}

	if (vm->istack.top == 0) {
		fprintf(io, "    inst: <s_empty>\n");
	} else {
		fprintf(io, "    inst: | %08x | 0\n", vm->istack.val[0]);
		for (i = 1; i < vm->istack.top; i++)
			fprintf(io, "          | %08x | %u\n", vm->istack.val[i], i);
		fprintf(io, "          '----------'\n");
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

			if (*b == 'T') {
				ADVANCE;
				n += snprintf(NULL, 0, "%s", vm->topic);
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
			ADVANCE;
			/* first, check for special registers */
			if (*b == 'T') {
				s += snprintf(s, n - (s - s1) + 1, "%s", vm->topic);
				ADVANCE;
				a = b;
				continue;
			}
			         /* [    */
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

static int s_copyfile(FILE *src, FILE *dst)
{
	rewind(src);
	char buf[8192];
	for (;;) {
		size_t nread = fread(buf, 1, 8192, src);
		if (nread == 0) {
			if (feof(src)) break;
			logger(LOG_ERR, "read error: %s", strerror(errno));
			fclose(dst);
			return 1;
		}

		size_t nwritten = fwrite(buf, 1, nread, dst);
		if (nwritten != nread) {
			logger(LOG_ERR, "read error: %s", strerror(errno));
			fclose(dst);
			return 1;
		}
	}

	return 0;
}

static char* s_remote_sha1(vm_t *vm, const char *key)
{
	if (!vm->aux.remote) return NULL;

	int rc;
	pdu_t *pdu;
	char *s;

	rc = pdu_send_and_free(pdu_make("FILE", 1, key), vm->aux.remote);
	if (rc != 0) return NULL;

	pdu = pdu_recv(vm->aux.remote);
	if (!pdu) {
		logger(LOG_ERR, "remote.sh1 - failed: %s", zmq_strerror(errno));
		return NULL;
	}
	if (strcmp(pdu_type(pdu), "ERROR") == 0) {
		s = pdu_string(pdu, 1);
		logger(LOG_ERR, "remote.sha1 - protocol violation: %s", s);
		free(s); pdu_free(pdu);
		return NULL;
	}
	if (strcmp(pdu_type(pdu), "SHA1") == 0) {
		s = pdu_string(pdu, 1);
		pdu_free(pdu);
		return s;
	}

	logger(LOG_ERR, "remote.sha1 - unexpected reply PDU [%s]", pdu_type(pdu));
	pdu_free(pdu);
	return NULL;
}

static int s_remote_file(vm_t *vm, const char *target, const char *temp)
{
	int rc, n = 0;
	char *s;
	pdu_t *reply;

	FILE *tmpf = tmpfile();
	if (!tmpf) {
		logger(LOG_ERR, "remote.file failed to create temporary file: %s",
			strerror(errno));
		return 1;
	}

	for (;;) {
		s = string("%i", n++);
		rc = pdu_send_and_free(pdu_make("DATA", 1, s), vm->aux.remote);
		free(s);
		if (rc != 0) return 1;

		reply = pdu_recv(vm->aux.remote);
		if (!reply) {
			logger(LOG_ERR, "remote.file failed: %s", zmq_strerror(errno));
			if (tmpf) fclose(tmpf);
			return 1;
		}
		logger(LOG_DEBUG, "remote.file received a %s PDU", pdu_type(reply));

		if (strcmp(pdu_type(reply), "EOF") == 0) {
			pdu_free(reply);
			break;
		}
		if (strcmp(pdu_type(reply), "BLOCK") != 0) {
			pdu_free(reply);
			logger(LOG_ERR, "remote.file - protocol violation: received a %s PDU (expected a BLOCK)", pdu_type(reply));
			if (tmpf) fclose(tmpf);
			return 1;
		}

		char *data = pdu_string(reply, 1);
		fprintf(tmpf, "%s", data);
		free(data);
		pdu_free(reply);
	}

	char *difftool = hash_get(&vm->pragma, "difftool");
	if (difftool) {
		rewind(tmpf);
		logger(LOG_DEBUG, "Running diff.tool: `%s NEWFILE OLDFILE'", difftool);
		FILE *out = tmpfile();
		if (!out) {
			logger(LOG_ERR, "Failed to open a temporary file difftool output: %s", strerror(errno));
		} else {
			FILE *err = tmpfile();
			runner_t runner = {
				.in  = tmpf,
				.out = out,
				.err = err,
				.uid = 0,
				.gid = 0,
			};

			char *diffcmd = string("%s %s -", difftool, target);
			rc = run2(&runner, "/bin/sh", "-c", diffcmd, NULL);
			if (rc < 0)
				logger(LOG_ERR, "`%s' killed or otherwise terminated abnormally");
			else if (rc == 127)
				logger(LOG_ERR, "`%s': command not found");
			else
				logger(LOG_DEBUG, "`%s' exited %i", diffcmd, rc);

			if (cw_logio(LOG_INFO, "%s", out) != 0)
				logger(LOG_ERR, "Failed to read standard output from difftool `%s': %s", diffcmd, strerror(errno));
			if (cw_logio(LOG_ERR, "diftool: %s", err) != 0)
				logger(LOG_ERR, "Failed to read standard error from difftool `%s': %s", diffcmd, strerror(errno));

			fclose(out);
			fclose(err);
			free(diffcmd);
		}
	}
	FILE *dst = fopen(temp, "w");
	if (!dst) {
		logger(LOG_ERR, "copyfile failed: %s", strerror(errno));
		fclose(tmpf);
		return 1;
	}

	if (strcmp(target, temp) != 0) {
		struct stat st;
		if (lstat(target, &st) == 0) {
			rc = fchown(fileno(dst), st.st_uid, st.st_gid);
			rc = fchmod(fileno(dst), st.st_mode);
		}
	}

	rc = s_copyfile(tmpf, dst);
	fclose(dst);
	fclose(tmpf);
	return rc;
}

static int s_copy(const char *from, const char *to)
{
	int src, dst;
	src = open(from, O_RDONLY);
	if (src < 0) return 1;

	dst = open(to, O_WRONLY|O_CREAT|O_TRUNC, 0777);
	if (dst < 0) {
		close(src);
		return 1;
	}

	int n = cw_cat(src, dst);
	close(src); close(dst);
	return n;
}

static char * s_fs_get(const char *path)
{
	/* RDWR, even though we won't write to it.
	   otherwise, we can open a directory and lseek
	   will fail mysteriously */
	int fd = open(path, O_RDWR);
	if (fd < 0) return NULL;

	off_t size = lseek(fd, 0, SEEK_END);
	if (size == (off_t)(-1)) {
		fprintf(stderr, "-size; bailing\n");
		close(fd);
		return NULL;
	}
	if (lseek(fd, 0, SEEK_SET) != 0) {
		close(fd);
		return NULL;
	}

	char *s = vmalloc(size * (sizeof(char) + 1));
	char *p = s;

	while (p < s + size) {
		long want = size - (p - s);
		if (want > 8192) want = 8192;

		size_t nread = read(fd, p, want);
		if (nread <= 0) {
			close(fd);
			free(s);
			return NULL;
		}

		p += nread;
	}

	close(fd);
	return s;
}

static int s_fs_put(const char *path, const char *contents)
{
	int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) return 1;

	size_t size = strlen(contents);
	const char *p = contents;
	while (p < contents + size) {
		size_t want = size - (p - contents);
		if (want > 8192) want = 8192;

		size_t wrote = write(fd, p, want);
		if (wrote <= 0) {
			close(fd);
			return 1;
		}

		p += wrote;
	}

	close(fd);
	return 0;
}

/************************************************************************/

#define ARG0(s) do { if ( vm->f1 ||  vm->f2) B_ERR(s " takes no operands");            } while (0)
#define ARG1(s) do { if (!vm->f1 ||  vm->f2) B_ERR(s " requires exactly one operand"); } while (0)
#define ARG2(s) do { if (!vm->f1 || !vm->f2) B_ERR(s " requires two operands");        } while (0)
#define REGISTER1(s) do { if (!is_register(vm->f1)) B_ERR(s " must be given a register as its first operand"); \
                          if (vm->oper1 > NREGS)    B_ERR(s " operand 1 register index %i out of bounds", vm->oper1); } while (0)
#define REGISTER2(s) do { if (!is_register(vm->f2)) B_ERR(s " must be given a register as its second operand"); \
                          if (vm->oper2 > NREGS)    B_ERR(s " operand 2 register index %i out of bounds", vm->oper2); } while (0)
#define B_ERR(...) do { \
	fprintf(vm->stderr, "pendulum bytecode error (0x%04x): ", vm->pc); \
	fprintf(vm->stderr, __VA_ARGS__); \
	fprintf(vm->stderr, "\n"); \
	vm->stop = 0; vm->acc = 1; \
} while (0)

static void op_noop(vm_t *vm) { }

static void op_push(vm_t *vm)
{
	ARG1("push");
	s_push(vm, &vm->dstack, VAL1(vm));
}

static void op_pop(vm_t *vm)
{
	ARG1("pop");
	REGISTER1("pop");

	REG1(vm) = s_pop(vm, &vm->dstack);
}

static void op_set(vm_t *vm)
{
	ARG2("set");
	REGISTER1("set");

	REG1(vm) = VAL2(vm);
}

static void op_swap(vm_t *vm)
{
	ARG2("swap");
	REGISTER1("swap");
	REGISTER2("swap");

	if (vm->oper1 == vm->oper2)
		B_ERR("swap requires distinct registers");

	REG1(vm) ^= REG2(vm);
	REG2(vm) ^= REG1(vm);
	REG1(vm) ^= REG2(vm);
}

static void op_pragma(vm_t *vm)
{
	ARG2("pragma");
	const char *v = STR1(vm);

	if (strcmp(v, "test") == 0) {
		vm->stderr = strcmp(STR2(vm), "on") == 0 ? stdout : stderr;

	} else if (strcmp(v, "trace") == 0) {
		vm->trace = strcmp(STR2(vm), "on") == 0;

	} else {
		/* set the value in the pragma hash */
		hash_set(&vm->pragma, v, (void *)STR2(vm));
	}
}

static void op_property(vm_t *vm)
{
	ARG2("property");
	REGISTER2("property");
	const char *v = hash_get(&vm->props, STR1(vm));
	if (v) {
		REG2(vm) = vm_heap_strdup(vm, v);
		vm->acc = 0;
	} else {
		vm->acc = 1;
	}
}

static void op_anno(vm_t *vm) { }

static void op_acc(vm_t *vm)
{
	ARG1("acc");
	REGISTER1("acc");

	REG1(vm) = vm->acc;
}

static void op_add(vm_t *vm)
{
	ARG2("add");
	REGISTER1("add");

	REG1(vm) += VAL2(vm);
}

static void op_sub(vm_t *vm)
{
	ARG2("sub");
	REGISTER1("sub");

	REG1(vm) -= VAL2(vm);
}

static void op_mult(vm_t *vm)
{
	ARG2("mult");
	REGISTER1("mult");

	REG1(vm) *= VAL2(vm);
}

static void op_div(vm_t *vm)
{
	ARG2("div");
	REGISTER1("div");

	REG1(vm) /= VAL2(vm);
}

static void op_mod(vm_t *vm)
{
	ARG2("mod");
	REGISTER1("mod");

	REG1(vm) %= VAL2(vm);
}

static void op_call(vm_t *vm)
{
	ARG1("call");
	if (!is_address(vm->f1))
		B_ERR("call requires an address for operand 1");

	s_save_state(vm);
	s_push(vm, &vm->istack, vm->pc);
	vm->pc = vm->oper1;
}

static void op_try(vm_t *vm)
{
	ARG1("try");
	if (!is_address(vm->f1))
		B_ERR("try requires an address for operand 1");

	s_save_state(vm);

	s_push(vm, &vm->tstack, vm->tryc);
	vm->tryc = vm->pc;

	s_push(vm, &vm->istack, vm->pc);
	vm->pc = vm->oper1;
}

static void op_ret(vm_t *vm)
{
	if (vm->f1) {
		ARG1("ret");
		vm->acc = VAL1(vm);
	} else {
		ARG0("ret");
	}

	if (s_empty(&vm->istack)) {
		/* last RET == HALT */
		vm->stop = 1;
		return;
	}

	vm->pc = s_pop(vm, &vm->istack);
	if (vm->tryc == vm->pc)
		vm->tryc = s_pop(vm, &vm->tstack);
	s_restore_state(vm);
}

static void op_bail(vm_t *vm)
{
	ARG1("bail");
	vm->acc = VAL1(vm);

	if (vm->tryc == 0) {
		/* last BAIL == HALT */
		vm->stop = 1;
		return;
	}

	while (vm->pc != vm->tryc) {
		vm->pc = s_pop(vm, &vm->istack);
		s_restore_state(vm);
	}

	vm->tryc = s_pop(vm, &vm->tstack);
}

static void op_eq(vm_t *vm)
{
	ARG2("eq");
	vm->acc = (VAL1(vm) == VAL2(vm)) ? 0 : 1;
}

static void op_gt(vm_t *vm)
{
	ARG2("gt");
	vm->acc = (VAL1(vm) >  VAL2(vm)) ? 0 : 1;
}

static void op_gte(vm_t *vm)
{
	ARG2("gte");
	vm->acc = (VAL1(vm) >= VAL2(vm)) ? 0 : 1;
}

static void op_lt(vm_t *vm)
{
	ARG2("lt");
	vm->acc = (VAL1(vm) <  VAL2(vm)) ? 0 : 1;
}

static void op_lte(vm_t *vm)
{
	ARG2("lte");
	vm->acc = (VAL1(vm) <= VAL2(vm)) ? 0 : 1;
}

static void op_streq(vm_t *vm)
{
	ARG2("streq");
	vm->acc = strcmp(STR1(vm), STR2(vm)) == 0 ? 0 : 1;
}

static void op_jmp(vm_t *vm)
{
	ARG1("jmp");
	vm->pc = vm->oper1;
}

static void op_jz(vm_t *vm)
{
	ARG1("jz");
	if (vm->acc == 0) vm->pc = VAL1(vm);
}

static void op_jnz(vm_t *vm)
{
	ARG1("jnz");
	if (vm->acc != 0) vm->pc = VAL1(vm);
}

static void op_string(vm_t *vm)
{
	ARG2("str");
	REGISTER2("str");

	REG2(vm) = vm_sprintf(vm, STR1(vm));
}

static void op_print(vm_t *vm)
{
	ARG1("print");
	vm_fprintf(vm, vm->stdout, STR1(vm));
}

static void op_error(vm_t *vm)
{
	ARG1("error");
	vm_fprintf(vm, vm->stderr, STR1(vm));
	fprintf(vm->stderr, "\n");
}

static void op_perror(vm_t *vm)
{
	ARG1("perror");
	vm_fprintf(vm, vm->stderr, STR1(vm));
	fprintf(vm->stderr, ": (%i) %s\n", errno, strerror(errno));
}

static void op_syslog(vm_t *vm)
{
	ARG2("syslog");
	char *s = _sprintf(vm, STR2(vm));
	logger(log_level_number(STR1(vm)), s); free(s);
}

static void op_flag(vm_t *vm)
{
	ARG1("flag");
	hash_set(&vm->flags, STR1(vm), "Y");
}

static void op_unflag(vm_t *vm)
{
	ARG1("unflag");
	hash_set(&vm->flags, STR1(vm), NULL);
}

static void op_flagged_p(vm_t *vm)
{
	ARG1("flagged?");
	vm->acc = hash_get(&vm->flags, STR1(vm)) ? 0 : 1;
}

static void op_fs_stat(vm_t *vm)
{
	ARG1("fs.stat");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
}

static void op_fs_type(vm_t *vm)
{
	ARG2("fs.type");
	REGISTER2("fs.type");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc != 0) {
		REG2(vm) = vm_heap_strdup(vm, "non-existent file");
	} else if (S_ISREG(vm->aux.stat.st_mode)) {
		REG2(vm) = vm_heap_strdup(vm, "regular file");
	} else if (S_ISLNK(vm->aux.stat.st_mode)) {
		REG2(vm) = vm_heap_strdup(vm, "symbolic link");
	} else if (S_ISDIR(vm->aux.stat.st_mode)) {
		REG2(vm) = vm_heap_strdup(vm, "directory");
	} else if (S_ISCHR(vm->aux.stat.st_mode)) {
		REG2(vm) = vm_heap_strdup(vm, "character device");
	} else if (S_ISBLK(vm->aux.stat.st_mode)) {
		REG2(vm) = vm_heap_strdup(vm, "block device");
	} else if (S_ISFIFO(vm->aux.stat.st_mode)) {
		REG2(vm) = vm_heap_strdup(vm, "FIFO pipe");
	} else if (S_ISSOCK(vm->aux.stat.st_mode)) {
		REG2(vm) = vm_heap_strdup(vm, "UNIX domain socket");
	} else {
		REG2(vm) = vm_heap_strdup(vm, "unknown file");
	}
}

static void op_fs_file_p(vm_t *vm)
{
	ARG1("fs.file?");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) vm->acc = S_ISREG(vm->aux.stat.st_mode) ? 0 : 1;
}

static void op_fs_symlink_p(vm_t *vm)
{
	ARG1("fs.symlink?");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) vm->acc = S_ISLNK(vm->aux.stat.st_mode) ? 0 : 1;
}

static void op_fs_dir_p(vm_t *vm)
{
	ARG1("fs.dir?");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) vm->acc = S_ISDIR(vm->aux.stat.st_mode) ? 0 : 1;
}

static void op_fs_chardev_p(vm_t *vm)
{
	ARG1("fs.chardev?");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) vm->acc = S_ISCHR(vm->aux.stat.st_mode) ? 0 : 1;
}

static void op_fs_blockdev_p(vm_t *vm)
{
	ARG1("fs.blockdev?");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) vm->acc = S_ISBLK(vm->aux.stat.st_mode) ? 0 : 1;
}

static void op_fs_fifo_p(vm_t *vm)
{
	ARG1("fs.fifo?");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) vm->acc = S_ISFIFO(vm->aux.stat.st_mode) ? 0 : 1;
}

static void op_fs_socket_p(vm_t *vm)
{
	ARG1("fs.socket?");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) vm->acc = S_ISSOCK(vm->aux.stat.st_mode) ? 0 : 1;
}

static void op_fs_readlink(vm_t *vm)
{
	ARG2("fs.readlink");
	REGISTER2("fs.readlink");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc != 0) return;

	vm->acc = S_ISLNK(vm->aux.stat.st_mode) ? 0 : 1;
	if (vm->acc != 0) return;

	char *s = vmalloc((vm->aux.stat.st_size + 1) * sizeof(char));
	if (readlink(STR1(vm), s, vm->aux.stat.st_size) != vm->aux.stat.st_size) {
		free(s);
		vm->acc = 1;
		errno = ENOBUFS;
		return;
	}
	if (vm->acc != 0) return;

	REG2(vm) = vm_heap_string(vm, s);
}

static void op_fs_dev(vm_t *vm)
{
	ARG2("fs.dev");
	REGISTER2("fs.dev");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_dev;
}

static void op_fs_inode(vm_t *vm)
{
	ARG2("fs.inode");
	REGISTER2("fs.inode");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_ino;
}

static void op_fs_mode(vm_t *vm)
{
	ARG2("fs.mode");
	REGISTER2("fs.mode");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_mode & 07777;
}

static void op_fs_nlink(vm_t *vm)
{
	ARG2("fs.nlink");
	REGISTER2("fs.nlink");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_nlink;
}

static void op_fs_uid(vm_t *vm)
{
	ARG2("fs.uid");
	REGISTER2("fs.uid");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_uid;
}

static void op_fs_gid(vm_t *vm)
{
	ARG2("fs.gid");
	REGISTER2("fs.gid");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_gid;
}

static void op_fs_major(vm_t *vm)
{
	ARG2("fs.major");
	REGISTER2("fs.major");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = major(vm->aux.stat.st_rdev);
}

static void op_fs_minor(vm_t *vm)
{
	ARG2("fs.minor");
	REGISTER2("fs.minor");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = minor(vm->aux.stat.st_rdev);
}

static void op_fs_size(vm_t *vm)
{
	ARG2("fs.size");
	REGISTER2("fs.size");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_size;
}

static void op_fs_atime(vm_t *vm)
{
	ARG2("fs.atime");
	REGISTER2("fs.atime");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_atime;
}

static void op_fs_mtime(vm_t *vm)
{
	ARG2("fs.mtime");
	REGISTER2("fs.mtime");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_mtime;
}

static void op_fs_ctime(vm_t *vm)
{
	ARG2("fs.ctime");
	REGISTER2("fs.ctime");

	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc == 0) REG2(vm) = vm->aux.stat.st_ctime;
}

static void op_fs_touch(vm_t *vm)
{
	ARG1("fs.touch");
	vm->acc = close(open(STR1(vm), O_CREAT, 0666));
}

static void op_fs_mkdir(vm_t *vm)
{
	ARG1("fs.mkdir");
	vm->acc = mkdir(STR1(vm), 0777);
}

static void op_fs_symlink(vm_t *vm)
{
	ARG2("fs.symlink");
	vm->acc = symlink(STR1(vm), STR2(vm));
}

static void op_fs_link(vm_t *vm)
{
	ARG2("fs.link");
	vm->acc = lstat(STR1(vm), &vm->aux.stat);
	if (vm->acc != 0) return;

	if (S_ISDIR(vm->aux.stat.st_mode)) {
		errno = EISDIR;
		vm->acc = 1;
		return;
	}

	vm->acc = link(STR1(vm), STR2(vm));
}

static void op_fs_unlink(vm_t *vm)
{
	ARG1("fs.unlink");
	vm->acc = unlink(STR1(vm));
}

static void op_fs_rmdir(vm_t *vm)
{
	ARG1("fs.rmdir");
	vm->acc = rmdir(STR1(vm));
}

static void op_fs_rename(vm_t *vm)
{
	ARG2("fs.rename");
	vm->acc = rename(STR1(vm), STR2(vm));
}

static void op_fs_copy(vm_t *vm)
{
	ARG2("fs.copy");
	vm->acc = s_copy(STR1(vm), STR2(vm));
}

static void op_fs_chown(vm_t *vm)
{
	ARG2("fs.chown");
	vm->acc = lchown(STR1(vm), VAL2(vm), -1);
}

static void op_fs_chgrp(vm_t *vm)
{
	ARG2("fs.chgrp");
	vm->acc = lchown(STR1(vm), -1, VAL2(vm));
}

static void op_fs_chmod(vm_t *vm)
{
	ARG2("fs.chmod");
	vm->acc = chmod(STR1(vm), VAL2(vm) & 07777);
}

static void op_fs_sha1(vm_t *vm)
{
	ARG2("fs.sha1");
	REGISTER2("fs.sha1");
	vm->acc = sha1_file(STR1(vm), &vm->aux.sha1);
	REG2(vm) = vm_heap_strdup(vm, vm->aux.sha1.hex);
}

static void op_fs_get(vm_t *vm)
{
	ARG2("fs.get");
	REGISTER2("fs.get");
	char *s = s_fs_get(STR1(vm));
	vm->acc = s ? 0 : 1;
	if (!s) return;

	REG2(vm) = vm_heap_string(vm, s);
}

static void op_fs_put(vm_t *vm)
{
	ARG2("fs.put");
	vm->acc = s_fs_put(STR1(vm), STR2(vm));
}

static void op_fs_opendir(vm_t *vm)
{
	ARG2("fs.opendir");
	REGISTER2("fs.opendir");

	int i;
	for (i = 0; i < VM_MAX_OPENDIRS; i++) {
		if (vm->aux.dirs[i].paths) continue;

		DIR *dir = opendir(STR1(vm));
		if (!dir) {
			vm->acc = 1;
			return;
		}
		vm->acc = 0;

		vm->aux.dirs[i].i     = 0;
		vm->aux.dirs[i].paths = strings_new(NULL);
		struct dirent *D;
		while ((D = readdir(dir)))
			if (strcmp(D->d_name, ".") != 0 && strcmp(D->d_name, "..") != 0)
				strings_add(vm->aux.dirs[i].paths, strdup(D->d_name));

		strings_sort(vm->aux.dirs[i].paths, STRINGS_ASC);
		vm->r[vm->oper2] = i;
		return;
	}
	errno = ENFILE;
	vm->acc = 1;
}

static void op_fs_readdir(vm_t *vm)
{
	ARG2("fs.readdir");
	REGISTER2("fs.readdir");

	int d = VAL1(vm);
	if (d < 0 || d > VM_MAX_OPENDIRS
	 || !vm->aux.dirs[d].paths
	 || vm->aux.dirs[d].i == vm->aux.dirs[d].paths->num) {
		vm->acc = 1;
		return;
	}

	vm->acc = 0;
	vm->r[vm->oper2] = vm_heap_strdup(vm,
		vm->aux.dirs[d].paths->strings[vm->aux.dirs[d].i++]);
}

static void op_fs_closedir(vm_t *vm)
{
	ARG1("fs.closedir");
	REGISTER1("fs.closedir");

	int d = VAL1(vm);
	if (d < 0 || d >= VM_MAX_OPENDIRS) {
		vm->acc = 1;
		return;
	}
	strings_free(vm->aux.dirs[d].paths);
	vm->aux.dirs[d].paths = NULL;
	vm->acc = 0;
}

static void op_authdb_open(vm_t *vm)
{
	ARG0("authdb.open");

	authdb_close(vm->aux.authdb);
	vm->aux.authdb = authdb_read(hash_get(&vm->pragma, "authdb.root"), AUTHDB_ALL);
	vm->acc = vm->aux.authdb != NULL ? 0 : 1;
}

static void op_authdb_save(vm_t *vm)
{
	ARG0("authdb.save");
	vm->acc = authdb_write(vm->aux.authdb);
}

static void op_authdb_close(vm_t *vm)
{
	ARG0("authdb.close");
	authdb_close(vm->aux.authdb);
	vm->aux.authdb = NULL;
	vm->acc = 0;
}

static void op_authdb_nextuid(vm_t *vm)
{
	ARG2("authdb.nextuid");
	REGISTER2("authdb.nextuid");
	vm->acc = authdb_nextuid(vm->aux.authdb, VAL1(vm));
	if (vm->acc < 65536) {
		REG2(vm) = vm->acc;
		vm->acc = 0;
	}
}

static void op_authdb_nextgid(vm_t *vm)
{
	ARG2("authdb.nextgid");
	REGISTER2("authdb.nextuid");
	vm->acc = authdb_nextgid(vm->aux.authdb, VAL1(vm));
	if (vm->acc < 65536) {
		REG2(vm) = vm->acc;
		vm->acc = 0;
	}
}

static void op_user_find(vm_t *vm)
{
	ARG1("user.find");
	if (!vm->aux.authdb) {
		vm->acc = 1;
		return;
	}
	vm->aux.user = user_find(vm->aux.authdb, STR1(vm), -1);
	vm->acc = vm->aux.user ? 0 : 1;
}

static void op_user_get(vm_t *vm)
{
	ARG2("user.get");
	REGISTER2("user.get");
	if (!vm->aux.user) {
		vm->acc = 1;
		return;
	}

	vm->acc = 0;
	const char *v = STR1(vm);

	if (strcmp(v, "uid") == 0) {
		REG2(vm) = vm->aux.user->uid;

	} else if (strcmp(v, "gid") == 0) {
		REG2(vm) = vm->aux.user->gid;

	} else if (strcmp(v, "username") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.user->name);

	} else if (strcmp(v, "comment") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.user->comment);

	} else if (strcmp(v, "home") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.user->home);

	} else if (strcmp(v, "shell") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.user->shell);

	} else if (strcmp(v, "password") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.user->clear_pass);

	} else if (strcmp(v, "pwhash") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.user->crypt_pass);

	} else if (strcmp(v, "changed") == 0) {
		REG2(vm) = vm->aux.user->creds.last_changed;

	} else if (strcmp(v, "pwmin") == 0) {
		REG2(vm) = vm->aux.user->creds.min_days;

	} else if (strcmp(v, "pwmax") == 0) {
		REG2(vm) = vm->aux.user->creds.max_days;

	} else if (strcmp(v, "pwwarn") == 0) {
		REG2(vm) = vm->aux.user->creds.warn_days;

	} else if (strcmp(v, "inact") == 0) {
		REG2(vm) = vm->aux.user->creds.grace_period;

	} else if (strcmp(v, "expiry") == 0) {
		REG2(vm) = vm->aux.user->creds.expiration;

	} else {
		vm->acc = 1;
	}
}

static void op_user_set(vm_t *vm)
{
	ARG2("user.set");
	if (!vm->aux.user) {
		vm->acc = 1;
		return;
	}

	vm->acc = 0;
	const char *v = STR1(vm);

	if (strcmp(v, "uid") == 0) {
		vm->aux.user->uid = VAL2(vm);

	} else if (strcmp(v, "gid") == 0) {
		vm->aux.user->gid = VAL2(vm);

	} else if (strcmp(v, "username") == 0) {
		free(vm->aux.user->name);
		vm->aux.user->name = strdup(STR2(vm));

	} else if (strcmp(v, "comment") == 0) {
		free(vm->aux.user->comment);
		vm->aux.user->comment = strdup(STR2(vm));

	} else if (strcmp(v, "home") == 0) {
		free(vm->aux.user->home);
		vm->aux.user->home = strdup(STR2(vm));

	} else if (strcmp(v, "shell") == 0) {
		free(vm->aux.user->shell);
		vm->aux.user->shell = strdup(STR2(vm));

	} else if (strcmp(v, "password") == 0) {
		free(vm->aux.user->clear_pass);
		vm->aux.user->clear_pass = strdup(STR2(vm));

	} else if (strcmp(v, "pwhash") == 0) {
		free(vm->aux.user->crypt_pass);
		vm->aux.user->crypt_pass = strdup(STR2(vm));

	} else if (strcmp(v, "changed") == 0) {
		vm->aux.user->creds.last_changed = VAL2(vm);

	} else if (strcmp(v, "pwmin") == 0) {
		vm->aux.user->creds.min_days = VAL2(vm);

	} else if (strcmp(v, "pwmax") == 0) {
		vm->aux.user->creds.max_days = VAL2(vm);

	} else if (strcmp(v, "pwwarn") == 0) {
		vm->aux.user->creds.warn_days = VAL2(vm);

	} else if (strcmp(v, "inact") == 0) {
		vm->aux.user->creds.grace_period = VAL2(vm);

	} else if (strcmp(v, "expiry") == 0) {
		vm->aux.user->creds.expiration = VAL2(vm);

	} else {
		vm->acc = 1;
	}
}

static void op_user_new(vm_t *vm)
{
	ARG0("user.new");
	vm->aux.user = user_add(vm->aux.authdb);
	vm->acc = vm->aux.user ? 0 : 1;
}

static void op_user_delete(vm_t *vm)
{
	ARG0("user.delete");
	if (!vm->aux.user) {
		vm->acc = 1;
		return;
	}
	user_remove(vm->aux.user);
	vm->acc = 0;
}

static void op_group_find(vm_t *vm)
{
	ARG1("group.find");
	if (!vm->aux.authdb) {
		vm->acc = 1;
		return;
	}
	vm->aux.group = group_find(vm->aux.authdb, STR1(vm), -1);
	vm->acc = vm->aux.group ? 0 : 1;
}

static void op_group_get(vm_t *vm)
{
	ARG2("group.get");
	REGISTER2("group.get");
	if (!vm->aux.group) {
		vm->acc = 1;
		return;
	}

	vm->acc = 0;
	const char *v = STR1(vm);

	if (strcmp(v, "gid") == 0) {
		REG2(vm) = vm->aux.group->gid;

	} else if (strcmp(v, "name") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.group->name);

	} else if (strcmp(v, "password") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.group->clear_pass);

	} else if (strcmp(v, "pwhash") == 0) {
		REG2(vm) = vm_heap_strdup(vm, vm->aux.group->crypt_pass);

	} else {
		vm->acc = 1;
	}
}

static void op_group_set(vm_t *vm)
{
	ARG2("group.set");
	if (!vm->aux.group) {
		vm->acc = 1;
		return;
	}

	vm->acc = 0;
	const char *v = STR1(vm);

	if (strcmp(v, "gid") == 0) {
		vm->aux.group->gid = VAL2(vm);

	} else if (strcmp(v, "name") == 0) {
		free(vm->aux.group->name);
		vm->aux.group->name = strdup(STR2(vm));

	} else if (strcmp(v, "password") == 0) {
		free(vm->aux.group->clear_pass);
		vm->aux.group->clear_pass = strdup(STR2(vm));

	} else if (strcmp(v, "pwhash") == 0) {
		free(vm->aux.group->crypt_pass);
		vm->aux.group->crypt_pass = strdup(STR2(vm));

	} else {
		vm->acc = 1;
	}
}

static void op_group_new(vm_t *vm)
{
	ARG0("group.new");
	vm->aux.group = group_add(vm->aux.authdb);
	vm->acc = vm->aux.group ? 0 : 1;
}

static void op_group_delete(vm_t *vm)
{
	ARG0("group.delete");
	if (!vm->aux.group) {
		vm->acc = 1;
		return;
	}
	group_remove(vm->aux.group);
	vm->acc = 0;
}

static void op_group_has_p(vm_t *vm)
{
	ARG2("group.has?");
	vm->acc = 1;
}

static void op_group_join(vm_t *vm)
{
	ARG2("group.join");
	vm->acc = 1;
}

static void op_group_kick(vm_t *vm)
{
	ARG2("group.kick");
	vm->acc = 1;
}

static void op_augeas_init(vm_t *vm)
{
	ARG0("augeas.init");
	vm->aux.augeas = aug_init(
		hash_get(&vm->pragma, "augeas.root"),
		hash_get(&vm->pragma, "augeas.libs"),
		AUG_NO_STDINC|AUG_NO_LOAD|AUG_NO_MODL_AUTOLOAD);

	if (!vm->aux.augeas) {
		vm->acc = 1;
		return;
	}

	if (aug_set(vm->aux.augeas, "/augeas/load/Hosts/lens", "Hosts.lns") < 0
	 || aug_set(vm->aux.augeas, "/augeas/load/Hosts/incl", "/etc/hosts") < 0
	 || aug_load(vm->aux.augeas) != 0) {
		vm->acc = 1;
	} else {
		vm->acc = 0;
	}
}

static void op_augeas_done(vm_t *vm)
{
	ARG0("augeas.done");
	aug_close(vm->aux.augeas);
	vm->aux.augeas = NULL;
	vm->acc = 0;
}

static void op_augeas_perror(vm_t *vm)
{
	ARG1("augeas.perror");
	if (!vm->aux.augeas) return;
	vm_fprintf(vm, vm->stderr, STR1(vm));

	char **err = NULL;
	const char *value;
	int i, rc = aug_match(vm->aux.augeas, "/augeas//error", &err);
	fprintf(vm->stderr, ": found %u problem%s:\n", rc, rc == 1 ? "" : "s");
	for (i = 0; i < rc; i++) {
		aug_get(vm->aux.augeas, err[i], &value);
		fprintf(vm->stderr, "  %s: %s\n", err[i], value);
	}
	free(err);
}

static void op_augeas_write(vm_t *vm)
{
	ARG0("augeas.write");
	if (!vm->aux.augeas) {
		vm->acc = 1;
		return;
	}
	vm->acc = aug_save(vm->aux.augeas);
}

static void op_augeas_set(vm_t *vm)
{
	ARG2("augeas.set");
	if (!vm->aux.augeas) {
		vm->acc = 1;
		return;
	}
	vm->acc = aug_set(vm->aux.augeas, STR1(vm), STR2(vm));
}

static void op_augeas_get(vm_t *vm)
{
	ARG2("augeas.get");
	REGISTER2("augeas.get");
	if (!vm->aux.augeas) {
		vm->acc = 1;
		return;
	}
	const char *v;
	if (aug_get(vm->aux.augeas, STR1(vm), &v) == 1 && v) {
		REG2(vm) = vm_heap_strdup(vm, v);
		vm->acc = 0;
	} else {
		vm->acc = 1;
	}
}

static void op_augeas_find(vm_t *vm)
{
	ARG2("augeas.find");
	REGISTER2("augeas.find");
	vm->acc = 1;
	if (!vm->aux.augeas) return;

	char **r = NULL, *v;
	int rc = aug_match(vm->aux.augeas, STR1(vm), &r);
	if (rc == 1) {
		v = r[0];
		free(r);

		vm->acc = 0;
		REG2(vm) = vm_heap_string(vm, v);
		return;
	}
	while (rc > 0) free(r[rc--]);
	free(r);
}

static void op_augeas_remove(vm_t *vm)
{
	ARG1("augeas.remove");
	if (!vm->aux.augeas) {
		vm->acc = 1;
		return;
	}
	vm->acc = aug_rm(vm->aux.augeas, STR1(vm)) > 1 ? 0 : 1;
}

static void op_env_get(vm_t *vm)
{
	ARG2("env.get");
	REGISTER2("env.get");
	char *s = getenv(STR1(vm));
	if (s) {
		REG2(vm) = vm_heap_strdup(vm, s);
		vm->acc = 0;
	} else {
		vm->acc = 1;
	}
}

static void op_env_set(vm_t *vm)
{
	ARG2("env.set");
	vm->acc = setenv(STR1(vm), STR2(vm), 1);
}

static void op_env_unset(vm_t *vm)
{
	ARG1("env.unset");
	vm->acc = unsetenv(STR1(vm));
}

static void op_localsys(vm_t *vm)
{
	ARG2("localsys");
	REGISTER2("localsys");

	char execline[8192] = {0};
	runner_t runner = {
		.in  = NULL,
		.out = tmpfile(),
		.err = tmpfile(),
		.uid = geteuid(),
		.gid = getegid(),
	};

	char *s = _sprintf(vm, STR1(vm));
	char *cmd = string("%s %s", hash_get(&vm->pragma, "localsys.cmd"), s);
	vm->acc = run2(&runner, "/bin/sh", "-c", cmd, NULL);

	if (fgets(execline, sizeof(execline), runner.out)) {
		char *s = strchr(execline, '\n'); if (s) *s = '\0';
		REG2(vm) = vm_heap_strdup(vm, execline);
	}
	fclose(runner.out); runner.out = NULL;
	fclose(runner.err); runner.err = NULL;

	logger(LOG_INFO, "%s: `%s` exited %d: \"%s\"",
		(vm->topic ? vm->topic : "(no topic)"),
		cmd, vm->acc, execline);
	free(cmd);
	free(s);
}

static void op_runas_uid(vm_t *vm)
{
	ARG1("runas.uid");
	vm->aux.runas_uid = VAL1(vm);
	if (vm->aux.runas_uid < 0 || vm->aux.runas_uid > 65535)
		vm->aux.runas_uid = 0;
}

static void op_runas_gid(vm_t *vm)
{
	ARG1("runas.gid");
	vm->aux.runas_gid = VAL1(vm);
	if (vm->aux.runas_gid < 0 || vm->aux.runas_gid > 65535)
		vm->aux.runas_gid = 0;
}

static void op_exec(vm_t *vm)
{
	ARG2("exec");
	REGISTER2("exec");

	char execline[8192];
	runner_t runner = {
		.in  = NULL,
		.out = tmpfile(),
		.err = tmpfile(),
		.uid = vm->aux.runas_uid,
		.gid = vm->aux.runas_gid,
	};

	logger(LOG_DEBUG, "exec: running `/bin/ash -c \"%s\"`", STR1(vm));
	vm->acc = run2(&runner, "/bin/sh", "-c", STR1(vm), NULL);
	if (fgets(execline, sizeof(execline), runner.out)) {
		char *s = strchr(execline, '\n'); if (s) *s = '\0';
		REG2(vm) = vm_heap_strdup(vm, execline);
	}
	fclose(runner.out); runner.out = NULL;
	fclose(runner.err); runner.err = NULL;
}

static void op_dump(vm_t *vm)
{
	ARG0("dump");
	dump(stdout, vm);
}

static void op_halt(vm_t *vm)
{
	ARG0("halt");
	vm->stop = 1;
}

static void op_acl(vm_t *vm)
{
	ARG1("acl");
	acl_t *acl = acl_parse(STR1(vm));
	list_push(&vm->acl, &acl->l);
	vm->acc = 0;
}

static void op_show_acls(vm_t *vm)
{
	ARG0("show.acls");
	char *s;
	acl_t *acl;
	for_each_object(acl, &vm->acl, l) {
		fprintf(stdout, "%s\n", s = acl_string(acl));
		free(s);
	}
}

static void op_show_acl(vm_t *vm)
{
	ARG1("show.acl");
	char *s;
	acl_t *acl;
	const char *v;

	v = STR1(vm);
	for_each_object(acl, &vm->acl, l) {
		if (!acl_match(acl, v, NULL)) continue;
		fprintf(stdout, "%s\n", s = acl_string(acl));
		free(s);
	}
}

static void op_remote_live_p(vm_t *vm)
{
	ARG0("remote.live?");
	vm->acc = vm->aux.remote ? 0 : 1;
}

static void op_remote_sha1(vm_t *vm)
{
	ARG2("remote.sha1");
	REGISTER2("remote.sha1");
	char *s = s_remote_sha1(vm, STR1(vm));
	vm->acc = 1;
	if (s) {
		REG2(vm) = vm_heap_strdup(vm, s);
		vm->acc = 0;
	}
}

static void op_remote_file(vm_t *vm)
{
	ARG2("remote.file");
	vm->acc = s_remote_file(vm, STR1(vm), STR2(vm));
}

static void op_topic(vm_t *vm)
{
	ARG1("topic");
	vm->topic = STR1(vm);
	vm->topics++;
}

static void op_umask(vm_t *vm)
{
	ARG2("umask");
	REGISTER2("umask");
	vm->r[vm->oper2] = umask(VAL1(vm));
}

static void op_loglevel(vm_t *vm)
{
	ARG2("loglevel");
	REGISTER2("loglevel");

	const char *new = STR1(vm);
	int level = log_level(-1, strcmp(new, "") == 0 ? NULL : new);
	vm->r[vm->oper2] = vm_heap_strdup(vm, log_level_name(level));
}

static void op_geteuid(vm_t *vm)
{
	ARG1("geteuid");
	REGISTER1("geteuid");

	vm->r[vm->oper1] = geteuid();
	vm->acc = 0;
}

static void op_getegid(vm_t *vm)
{
	ARG1("getegid");
	REGISTER1("getegid");

	vm->r[vm->oper1] = getegid();
	vm->acc = 0;
}

/************************************************************************/

int vm_iscode(byte_t *code, size_t len)
{
	if ((!code)           /* gotta have a pointer to _something_ */
	 || (len < 4)     /* smallest bytecode image is 'pn<HALT>00' */
	 || (code[0] != 'p'  /* bytecode "magic number" is 0x70 0x68 */
	  || code[1] != 'n'))                 /* (which spells "pn") */
		return 0;

	return 1; /* OK! */
}

int vm_reset(vm_t *vm)
{
	assert(vm);
	memset(vm, 0, sizeof(vm_t));
	list_init(&vm->heap);
	return 0;
}

int vm_load(vm_t *vm, byte_t *code, size_t len)
{
	assert(vm);
	assert(code);
	assert(len > 3);
	assert(code[0] == 'p' && code[1] == 'n');

	/* default pragmas */
	hash_set(&vm->pragma, "authdb.root",  AUTHDB_ROOT);
	hash_set(&vm->pragma, "augeas.root",  AUGEAS_ROOT);
	hash_set(&vm->pragma, "augeas.libs",  AUGEAS_LIBS);
	hash_set(&vm->pragma, "localsys.cmd", "cw localsys");

	/* default properties */
	hash_set(&vm->props, "version", CLOCKWORK_VERSION);
	hash_set(&vm->props, "runtime", CLOCKWORK_RUNTIME);

	list_init(&vm->acl);

	vm->stdout = stdout;
	vm->stderr = stderr;
	s_push(vm, &vm->tstack, 0);

	vm->code = code;
	vm->codesize = len;

	/* determine the static boundary */
	byte_t f;
	code += 2; /* skip header */
	while (code < vm->code + len) {
		if (*code == OPx_EOF) {
			vm->static0 = code - vm->code;
			break;
		}

		code++; f = *code++;
		switch (HI_NYBLE(f)) {
		case 0:          break;
		case TYPE_EMBED: while (*code++); break;
		default:         code += 4;
		}
		switch (LO_NYBLE(f)) {
		case 0:          break;
		case TYPE_EMBED: while (*code++); break;
		default:         code += 4;
		}
	}

	return 0;
}

int vm_args(vm_t *vm, int argc, char **argv)
{
	assert(vm);
	int i;
	for (i = argc - 1; i >= 0; i--) {
		size_t n = strlen(argv[i]) + 1;
		heap_t *h = vm_heap_alloc(vm, n);
		memcpy(h->data, argv[i], n);
		s_push(vm, &vm->dstack, h->addr);
	}
	s_push(vm, &vm->dstack, argc);
	return 0;
}

int vm_exec(vm_t *vm)
{
	vm->pc = 2; /* skip the header */
again:
	while (!vm->stop) {
		vm->oper1 = vm->oper2 = 0;
		vm->op = vm->code[vm->pc];

		if (vm->ccovio)
			fprintf(vm->ccovio, "%08x %02x\n", vm->pc, vm->op);
		vm->pc++;

		vm->f1 = HI_NYBLE(vm->code[vm->pc]);
		vm->f2 = LO_NYBLE(vm->code[vm->pc]);
		if (vm->trace)
			fprintf(vm->stderr, "+%s [%02x]", OPCODES[vm->op], vm->code[vm->pc]);
		vm->pc++;

		if (vm->f2 && !vm->f1)
			B_ERR("corrupt operands mask detected; vm->f1=%02x, vm->f2=%02x", vm->f1, vm->f2);

		if (vm->f1 == TYPE_EMBED) {
			if (vm->trace)
				fprintf(vm->stderr, " <%s>", vm->code + vm->pc);
			while (vm->code[vm->pc++]);

		} else if (vm->f1) {
			vm->oper1 = DWORD(vm->code[vm->pc + 0],
			                  vm->code[vm->pc + 1],
			                  vm->code[vm->pc + 2],
			                  vm->code[vm->pc + 3]);
			vm->pc += 4;
			if (vm->trace)
				fprintf(vm->stderr, " %08x", vm->oper1);
		}

		if (vm->f2 == TYPE_EMBED) {
			if (vm->trace)
				fprintf(vm->stderr, " <%s>", vm->code + vm->pc);
			while (vm->code[vm->pc++]);

		} else if (vm->f2) {
			vm->oper2 = DWORD(vm->code[vm->pc + 0],
			                  vm->code[vm->pc + 1],
			                  vm->code[vm->pc + 2],
			                  vm->code[vm->pc + 3]);
			vm->pc += 4;
			if (vm->trace)
				fprintf(vm->stderr, " %08x", vm->oper2);
		}

		if (vm->trace)
			fprintf(vm->stderr, "\n");

		int i;
		for (i = 0; OPCODE_HANDLERS[i].handler; i++) {
			if (OPCODE_HANDLERS[i].opcode != vm->op) continue;
			(*OPCODE_HANDLERS[i].handler)(vm);
			if (vm->stop) return vm->acc;
			goto again;
		}
		B_ERR("unknown operand %02x", vm->op);
		return -1;
	}
	return vm->acc;
}

int vm_disasm(vm_t *vm, FILE *out)
{
	if (vm->codesize < 3) return 1;
	if (vm->code[0] != 'p') return 2;
	if (vm->code[1] != 'n') return 2;

	fprintf(out, "0x%08x: ", 0);
	fprintf(out, "%02x %02x\n", 'p', 'n');

	size_t i = 2;
	dword_t addr;

	while (i < vm->codesize) {
		addr = (dword_t)i;
		vm->op = vm->code[i++];
		if (vm->op == OPx_EOF && !vm->code[i]) {
			fprintf(out, "0x%08x: ", addr);
			fprintf(out, "%02x %02x\n", OPx_EOF, 0x00);
			i++;
			break;
		}
		vm->f1 = HI_NYBLE(vm->code[i]);
		vm->f2 = LO_NYBLE(vm->code[i]);

		if (vm->f2 && !vm->f1) {
			fprintf(out, "pendulum bytecode error: corrupt operands mask detected; vm->f1=%02x, vm->f2=%02x\n", vm->f1, vm->f2);
			return 1;
		}

		if (vm->op == OP_ANNO) {
			/* we output annotations differently */
			i++;

			/* oper1 */
			vm->oper1 = DWORD(vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3]);
			i += 4;

			char line[81] = {0};
			size_t pos;

			switch (vm->oper1) {
			case ANNO_MODULE:
				pos = strlen((char *)vm->code + i);
				if (pos > 80 - 6 - 6) /* 6 == strlen('--- [ ') */
					pos = 80 - 6 - 6; /*  and strlen(' ] ---') */
				memset(line, '=', 80);
				memcpy(line + 3, " [ ", 3);
				memcpy(line + 6, vm->code + i, pos);
				memcpy(line + 6 + pos, " ] ", 3);
				fprintf(out, "\n%s\n\n", line);
				break;

			case ANNO_USER:
				fprintf(out, "%46s;; %s\n", " ", vm->code + i);
				break;

			case ANNO_FUNCTION:
				fprintf(out, "%32s fn %s\n", " ", vm->code + i);
				break;

			default:
				fprintf(out, "%32s anno [%02x] \"%s\"\n", " ", vm->oper1, vm->code + i);
			}

			/* oper2 */
			while (vm->code[i++]);
			continue;
		}

		fprintf(out, "0x%08x: %02x %02x", addr, vm->op, vm->code[i++]);
		if (vm->f1 == TYPE_EMBED) {
			fprintf(out, "<%s> [00]\n", (char *)vm->code + i);
			while (vm->code[i++]);

		} else if (vm->f1) {
			fprintf(out, " [%02x %02x %02x %02x]",
				vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3]);
			vm->oper1 = DWORD(vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3]);
			i += 4;

		} else {
			fprintf(out, "%14s", " ");
		}
		fprintf(out, "  %12s", OPCODES[vm->op]);
		if (vm->f1) {
			switch (vm->f1) {
			case TYPE_LITERAL:  fprintf(out, " %u",     vm->oper1); break;
			case TYPE_REGISTER: fprintf(out, " %%%c",   vm->oper1 + 'a'); break;
			case TYPE_ADDRESS:  fprintf(out, " 0x%08x", vm->oper1);
			                    if (vm->oper1 > vm->static0) {
			                        fprintf(out, " ; \"");
			                        s_eprintf(out, (char *)vm->code + vm->oper1);
			                        fprintf(out, "\"");
			                    }
			                    break;
			case TYPE_EMBED:    break;
			default:            fprintf(out, " ; operand 1 unknown (0x%x/0x%x)",
			                                    vm->f1, vm->oper1);
			}

		}
		if (vm->f2 == TYPE_EMBED) {
			fprintf(out, "\n                  <%s> [00]\n", (char *)vm->code + i);
			while (vm->code[i++]);

		} else if (vm->f2) {

			fprintf(out, "\n                  [%02x %02x %02x %02x]  %12s",
				vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3], "");
			vm->oper2 = DWORD(vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3]);
			i += 4;
		}

		if (vm->f2) {
			switch (vm->f2) {
			case TYPE_LITERAL:  fprintf(out, " %u",     vm->oper2); break;
			case TYPE_REGISTER: fprintf(out, " %%%c",   vm->oper2 + 'a'); break;
			case TYPE_ADDRESS:  fprintf(out, " 0x%08x", vm->oper2);
			                    if (vm->oper2 > vm->static0) {
			                        fprintf(out, " ; \"");
			                        s_eprintf(out, (char *)vm->code + vm->oper2);
			                        fprintf(out, "\"");
			                    }
			                    break;
			case TYPE_EMBED:    break;
			default:            fprintf(out, " ; operand 2 unknown (0x%x/0x%x)",
			                                    vm->f2, vm->oper2);
			}
		}

		fprintf(out, "\n");
	}

	fprintf(out, "---\n");
	while (i < vm->codesize) {
		fprintf(out, "0x%08x: [", (dword_t)i);
		s_eprintf(out, (char *)(vm->code + i));
		fprintf(out, "]\n");
		i += strlen((char *)vm->code + i) + 1;
	}
	return 0;
}

int vm_done(vm_t *vm)
{
	heap_t *h, *tmp;
	for_each_object_safe(h, tmp, &vm->heap, l) {
		list_delete(&h->l);
		free(h->data);
		free(h);
	}

	hash_done(&vm->props,  0);
	hash_done(&vm->pragma, 0);
	hash_done(&vm->flags,  0);
	return 0;
}

/************************************************************************/

#define PNASM_LINE 8192

typedef struct {
	byte_t type;
	byte_t bintype;
	union {
		char     regname;   /* register (a, c, etc.) */
		dword_t  literal;   /* literal value         */
		char    *string;    /* string value          */
		dword_t  address;   /* memory address        */
		char    *label;     /* for labelled jumps    */
		char    *fnlabel;   /* for labelled jumps    */
		dword_t  offset;    /* for relative jumps    */
	} _;
} value_t;

#define SPECIAL_LABEL 1
#define SPECIAL_FUNC  2
typedef struct {
	byte_t  special;     /* identify special, non-code markers */
	char   *label;       /* special label */
	void   *fn;          /* link to the FN that starts scope */

	byte_t  op;          /* opcode number, for final encoding */
	value_t args[2];     /* operands */

	dword_t offset;      /* byte offset in opcode binary stream */
	list_t  l;
} op_t;

typedef struct {
	list_t   l;
	FILE    *io;
	char    *file;
	char    *name;

	int      line;
	int      token;
	char     value[PNASM_LINE];
	char     buffer[PNASM_LINE];
	char     raw[PNASM_LINE];
} asm_unit_t;

#define s_asm_unit(pna) list_tail(&(pna)->units, asm_unit_t, l)
static int s_asm_unit_push(asm_t *pna);
static int s_asm_unit_pop(asm_t *pna);
static op_t* s_asm_op(asm_t *pna, byte_t type);
static op_t* s_asm_annotate(asm_t *pna, dword_t anno, char *label);
static int s_asm_resolve(asm_t *pna, value_t *v, op_t *me);
static int s_asm_include(asm_t *pna, const char *module);
static int s_asm_lex(asm_t *pna);
static int s_asm_parse(asm_t *pna);
static int s_asm_bytecode(asm_t *pna);


static int s_asm_unit_push(asm_t *pna)
{
	assert(pna);
	asm_unit_t *u = vmalloc(sizeof(asm_unit_t));
	if (u) {
		list_init(&u->l);
		list_push(&pna->units, &u->l);
		return 0;
	}
	return 1;
}

static int s_asm_unit_pop(asm_t *pna)
{
	assert(pna);
	asm_unit_t *u = s_asm_unit(pna);
	if (!u) return 1;

	list_delete(&u->l);
	free(u);
	return 0;
}

static op_t* s_asm_op(asm_t *pna, byte_t type)
{
	op_t *op = vmalloc(sizeof(op_t));
	op->op = type;
	list_push(&pna->ops, &op->l);
	return op;
}

static op_t* s_asm_annotate(asm_t *pna, dword_t anno, char *label)
{
	op_t *op = s_asm_op(pna, OP_ANNO);
	op->args[0].type = VALUE_NUMBER;
	op->args[0]._.literal = anno;
	op->args[1].type = VALUE_EMBED;
	op->args[1]._.string = label;
	return op;
}

static int s_asm_resolve(asm_t *pna, value_t *v, op_t *me)
{
	byte_t *addr;
	size_t len;
	op_t *op, *fn;

	switch (v->type) {
	case VALUE_REGISTER:
		v->bintype = TYPE_REGISTER;
		v->_.literal -= 'a';
		return 0;

	case VALUE_NUMBER:
		v->bintype = TYPE_LITERAL;
		return 0;

	case VALUE_STRING:
		addr = hash_get(&pna->data.strings, v->_.string);
		if (!addr) {
			len = strlen(v->_.string) + 1;
			memcpy(pna->code + pna->data.fill, v->_.string, len);

			addr = pna->code + pna->data.fill;
			hash_set(&pna->data.strings, v->_.string, addr);
			pna->data.fill += len;
		}
		free(v->_.string);

		v->type = VALUE_ADDRESS;
		v->bintype = TYPE_ADDRESS;
		v->_.address = addr - pna->code;
		return 0;

	case VALUE_EMBED:
		v->bintype = TYPE_EMBED;
		return 0;

	case VALUE_ADDRESS:
		v->bintype = TYPE_ADDRESS;
		return 0;

	case VALUE_LABEL:
		fn = (op_t*)me->fn;
		for_each_object(op, &fn->l, l) {
			if (op->special == SPECIAL_FUNC) break;
			if (op->special != SPECIAL_LABEL) continue;
			if (strcmp(v->_.label, op->label) != 0) continue;

			free(v->_.label);
			v->type = VALUE_ADDRESS;
			v->bintype = TYPE_ADDRESS;
			v->_.address = op->offset;
			return 0;
		}
		logger(LOG_ERR, "asm (resolver): label %s not found in scope!", v->_.label);
		return 1;

	case VALUE_FNLABEL:
		for_each_object(op, &pna->ops, l) {
			if (op->special != SPECIAL_FUNC) continue;
			if (strcmp(v->_.fnlabel, op->label) != 0) continue;

			free(v->_.fnlabel);
			v->type = VALUE_ADDRESS;
			v->bintype = TYPE_ADDRESS;
			v->_.address = op->offset;
			return 0;
		}
		logger(LOG_ERR, "asm (resolver): function %s not defined!", v->_.fnlabel);
		return 1;

	case VALUE_OFFSET:
		for_each_object(op, &me->l, l) {
			if (op->special) continue;
			if (v->_.offset--) continue;

			v->type = VALUE_ADDRESS;
			v->bintype = TYPE_ADDRESS;
			v->_.address = op->offset;
			return 0;
		}
		return 1;
	}
	return 0;
}

static int s_asm_include(asm_t *pna, const char *module)
{
	assert(pna);
	asm_unit_t *u = s_asm_unit(pna);

	struct stat st;
	int i;
	for (i = 0; i < pna->include.paths->num; i++) {
		char *path = string("%s/%s.pn", pna->include.paths->strings[i], module);
		logger(LOG_DEBUG, "asm (preprocessor): checking for existence of `%s' module in file %s", module, path);
		if (stat(path, &st) != 0) {
			free(path);
			continue;
		}

		logger(LOG_DEBUG, "asm (preprocessor): found '%s' module in file `%s' (dev/ino:%u/%u)",
			module, path, st.st_dev, st.st_ino);
		char *key = string("%u:%u", st.st_dev, st.st_ino);
		if (hash_get(&pna->include.seen, key) != NULL) {
			free(path);
			free(key);
			return 0;
		}

		logger(LOG_DEBUG, "asm (preprocessor): '%s' is a new (never-before-included) file", path);
		hash_set(&pna->include.seen, key, "Y");
		free(key);

		if (s_asm_unit_push(pna) != 0) {
			free(path);
			return -1;
		}

		u = s_asm_unit(pna);
		u->file = path;
		u->io = fopen(u->file, "r");
		if (!u->io)
			return -1;

		u->name = string("module : %s", module);
		s_asm_annotate(pna, ANNO_MODULE, strdup(u->name));
		if (s_asm_parse(pna) != 0) /* recurse! */
			return -1;

		s_asm_unit_pop(pna);
		s_asm_annotate(pna, ANNO_MODULE, strdup(u->name));
		return 0;
	}
	logger(LOG_ERR, "asm (preprocessor): %s:%i: could not find module `%s' for #include", u->file, u->line, module);
	return -1;
}

#define SHIFTLINE do { \
	while (*b && isspace(*b)) b++; \
	memmove(u->buffer, b, PNASM_LINE - (b - u->buffer)); \
} while (0)
static int s_asm_lex(asm_t *pna)
{
	assert(pna);
	asm_unit_t *u = s_asm_unit(pna);

	char *a, *b;
	if (!*u->buffer || *u->buffer == ';') {
getline:
		if (!fgets(u->raw, PNASM_LINE, u->io))
			return 0;
		u->line++;

		a = u->raw;
		while (*a && isspace(*a)) a++;
		if (*a == ';')
			while (*a && *a != '\n') a++;
		while (*a && isspace(*a)) a++;
		if (!*a)
			goto getline;

		b = u->buffer;
		while ((*b++ = *a++));
	}
	u->token = 0;
	u->value[0] = '\0';

	b = u->buffer;
	while (*b && isspace(*b)) b++;
	a = b;

	if (*b == '#') { /* preprocesor directive */
		b++;
		while (!isspace(*b)) b++;
		*b++ = '\0';
		if (strcmp(a, "#include") == 0) {
			while (*b && isspace(*b)) *b++ = '\0';
			if (!*b) {
				logger(LOG_ERR, "%s:%i: '#include' directive requires an argument", u->file, u->line);
				pna->abort = 1;
				return 0;
			}
			a = b; while (!isspace(*b)) b++;
			*b++ = '\0';
			if (*b) {
				logger(LOG_ERR, "%s:%i: bad invocation of '#include' preprocessor directive", u->file, u->line);
				pna->abort = 1;
				return 0;
			}
			if (s_asm_include(pna, a) != 0) {
				pna->abort = 1;
				return 0;
			}
			goto getline;
		}
		logger(LOG_ERR, "%s:%i: unrecognized preprocessor directive '%s'", u->file, u->line, a);
		pna->abort = 1;
		return 0;
	}
	if (*b == '<') { /* start of <<EOF ? */
		if (*++b == '<') {
			b++;
			int lines = 0;
			char *marker = b, *s = NULL;
			/* start reading lines, until we hit our marker again */
			while ((fgets(u->raw, PNASM_LINE, u->io))) {
				lines++;
				if (strcmp(u->raw, marker) == 0) {
					*u->raw = *u->buffer = '\0';
					u->token = T_STRING;
					if (strlen(s) > PNASM_LINE - 1)
						logger(LOG_WARNING, "%s:%i: heredoc string literal is too long (>%u bytes)", u->file, u->line, PNASM_LINE - 1);
					strncpy(u->value, s, PNASM_LINE - 1);
					u->line += lines; /* after the error message! */
					free(s);
					return 1;
				}
				char *t = s;
				s = string("%s%s", t ? t : "", u->raw); free(t);
			}
			logger(LOG_ERR, "%s:%i: unterminated heredoc string literal (expecting '%s' marker never seen)", u->file, u->line, b);
			pna->abort = 1;
			return 0;
		}
		b = a;
	}
	if (*b == '%') { /* register */
		while (*b && !isspace(*b)) b++;
		if (!*b || isspace(*b)) {
			*b = '\0';
			char reg = b - a - 2 ? '0' : *(a+1);
			if (reg < 'a' || reg >= 'a'+NREGS) {
				logger(LOG_ERR, "%s:%i: unrecognized register address %s (%i)", u->file, u->line, a, reg);
				pna->abort = 1;
				return 0;
			}

			u->token = T_REGISTER;
			u->value[0] = reg;
			u->value[1] = '\0';

			b++;
			SHIFTLINE;
			return 1;
		}
		b = a;
	}

	if (*b == '+' || *b == '-') {
		b++;
		while (*b && isdigit(*b)) b++;
		if (!*b || isspace(*b)) {
			*b++ = '\0';

			u->token = T_OFFSET;
			memcpy(u->value, u->buffer, b-u->buffer);

			SHIFTLINE;
			return 1;
		}
		b = a;
	}

	if (isdigit(*b)) {
		if (*b == '0' && *(b+1) == 'x') {
			b += 2;
			while (*b && isxdigit(*b)) b++;
		} else {
			while (*b && isdigit(*b)) b++;
		}
		if (!*b || isspace(*b)) {
			*b++ = '\0';

			u->token = T_NUMBER;
			memcpy(u->value, u->buffer, b-u->buffer);

			SHIFTLINE;
			return 1;
		}
		b = a;
	}

	if (isalpha(*b)) {
		while (*b && !isspace(*b))
			b++;
		if (b > a && *(b-1) == ':') {
			*(b-1) = '\0';
			u->token = T_LABEL;
			memcpy(u->value, u->buffer, b-u->buffer);
			SHIFTLINE;
			return 1;
		}
		b = a;

		while (*b && (isalnum(*b) || *b == '.' || *b == '_' || *b == '?' || *b == '-' || *b == ':'))
			b++;
		*b++ = '\0';

		if (strcmp(a, "acl") == 0) {
			u->token = T_ACL;
			a = b;
			while (*b && *b != '\n') b++;
			*b = '\0';
			memcpy(u->value, a, b - a + 1);
			memset(u->buffer, 0, PNASM_LINE);
			return 1;
		}

		int i;
		for (i = 0; ASM[i]; i++) {
			if (strcmp(a, ASM[i]) != 0) continue;
			u->token = T_OPCODE;
			u->value[0] = T_OP_NOOP + i;
			u->value[1] = '\0';
			break;
		}

		if (strcmp(a, "fn") == 0) u->token = T_FUNCTION;

		if (!u->token) {
			u->token = T_IDENTIFIER;
			memcpy(u->value, u->buffer, b-u->buffer);
		}

		SHIFTLINE;
		return 1;
	}

	if (*b == '"') {
		b++; a = u->value;
		while (*b && *b != '"' && *b != '\r' && *b != '\n') {
			if (*b == '\\') {
				b++;
				switch (*b) {
				case 'n': *a = '\n'; break;
				case 'r': *a = '\r'; break;
				case 't': *a = '\t'; break;
				default:  *a = *b;
				}
				a++; b++;
			} else *a++ = *b++;
		}
		*a = '\0';
		if (*b == '"') b++;
		else logger(LOG_WARNING, "%s:%i: unterminated string literal", u->file, u->line);

		u->token = T_STRING;
		SHIFTLINE;
		return 1;
	}


	logger(LOG_ERR, "%s:%i: failed to parse '%s'", u->file, u->line, u->buffer);
	pna->abort = 1;
	return 0;
}
#undef SHIFTLINE

#define NEXT do { \
	if (!s_asm_lex(pna)) { \
		logger(LOG_CRIT, "%s:%i: unexpected end of configuration\n", u->file, u->line); \
		goto bail; \
	} \
} while (0)

#define ERROR(s) do { \
	logger(LOG_CRIT, "%s:%i: syntax error: %s", u->file, u->line, s); \
	goto bail; \
} while (0)

#define BADTOKEN(s) do { \
	logger(LOG_CRIT, "%s:%i: unexpected " s " '%s'", u->file, u->line, u->value); \
	goto bail; \
} while (0)

static int s_asm_parse(asm_t *pna)
{
	assert(pna);
	asm_unit_t *u = s_asm_unit(pna);
	assert(u);

	logger(LOG_DEBUG, "asm: starting to parse pendulum asm source file `%s'", u->file);

	op_t *op, *FN = NULL;
	int i, j;

	while (s_asm_lex(pna)) {
		op = vmalloc(sizeof(op_t));
		op->fn = FN;

		switch (u->token) {
		case T_FUNCTION:
			if (FN && list_tail(&pna->ops, op_t, l)->op != OP_RET
			       && list_tail(&pna->ops, op_t, l)->op != OP_BAIL) {
				op->op = OP_RET;
				list_push(&pna->ops, &op->l);
				op = vmalloc(sizeof(op_t));
				op->fn = FN;
			}
			FN = op;
			op->special = SPECIAL_FUNC;
			NEXT;
			if (u->token != T_IDENTIFIER)
				ERROR("unacceptable name for function");
			op->label = strdup(u->value);
			s_asm_annotate(pna, ANNO_FUNCTION, strdup(u->value));

			if (hash_get(&pna->funcs, u->value)) {
				logger(LOG_CRIT, "%s:%i: function `%s' redefined (previous definition was at %s)",
					u->file, u->line, u->value, hash_get(&pna->funcs, u->value));
				goto bail;
			}
			hash_set(&pna->funcs, u->value, string("%s:%i", u->file, u->line));

			break;

		case T_ACL:
			op->op = OP_ACL;
			op->args[0].type = VALUE_STRING;
			op->args[0]._.string = strdup(u->value);
			break;

		case T_LABEL:
			op->special = SPECIAL_LABEL;
			op->label = strdup(u->value);
			break;

		case T_OPCODE:
			for (i = 0; ASM_SYNTAX[i].token; i++) {
				if ((byte_t)u->value[0] != ASM_SYNTAX[i].token) continue;
				op->op = (byte_t)ASM_SYNTAX[i].opcode;

				for (j = 0; j < 2; j++) {
					if (ASM_SYNTAX[i].args[j] == ARG_NONE) break;
					NEXT;

					if (u->token == T_REGISTER && ASM_SYNTAX[i].args[j] & ARG_REGISTER) {
						op->args[j].type = VALUE_REGISTER;
						op->args[j]._.regname = u->value[0];

					} else if (u->token == T_NUMBER && ASM_SYNTAX[i].args[j] & ARG_NUMBER) {
						op->args[j].type = VALUE_NUMBER;
						op->args[j]._.literal = strtol(u->value, NULL, 0);

					} else if (u->token == T_STRING && ASM_SYNTAX[i].args[j] & ARG_STRING) {
						op->args[j].type = VALUE_STRING;
						op->args[j]._.string = strdup(u->value);

					} else if (u->token == T_STRING && ASM_SYNTAX[i].args[j] & ARG_EMBED) {
						op->args[j].type = VALUE_EMBED;
						op->args[j]._.string = strdup(u->value);

					} else if (u->token == T_IDENTIFIER && ASM_SYNTAX[i].args[j] & ARG_LABEL) {
						op->args[j].type = VALUE_LABEL;
						op->args[j]._.label = strdup(u->value);

					} else if (u->token == T_IDENTIFIER && ASM_SYNTAX[i].args[j] & ARG_FUNCTION) {
						op->args[j].type = VALUE_FNLABEL;
						op->args[j]._.fnlabel = strdup(u->value);

					} else if (u->token == T_IDENTIFIER && ASM_SYNTAX[i].args[j] & ARG_IDENTIFIER) {
						op->args[j].type = VALUE_STRING;
						op->args[j]._.string = strdup(u->value);

					} else if (u->token == T_OFFSET && ASM_SYNTAX[i].args[j] & ARG_LABEL) {
						op->args[j].type = VALUE_OFFSET;
						op->args[j]._.offset = strtol(u->value, NULL, 10);

					} else {
						logger(LOG_CRIT, "%s: %i: invalid form; expected `%s`",
							u->file, u->line, ASM_SYNTAX[i].usage);
						goto bail;
					}
				}

				/* compile-time fixups */
				if (op->op == OP_ANNO) {
					/* all parsed annotations are ANNO_USER annotations */
					memcpy(&op->args[1], &op->args[0], sizeof(value_t));
					op->args[0].type = VALUE_NUMBER;
					op->args[0]._.literal = ANNO_USER;
				}

				break;
			}
			break;

		case T_REGISTER:   BADTOKEN("register reference");
		case T_IDENTIFIER: BADTOKEN("identifier");
		case T_OFFSET:     BADTOKEN("offset");
		case T_NUMBER:     BADTOKEN("numeric literal");
		case T_STRING:     BADTOKEN("string literal");

		default:
			ERROR("unhandled token type");
		}

		list_push(&pna->ops, &op->l);
	}

	if (pna->abort) {
		logger(LOG_WARNING, "asm: aborting");
		return 1;
	}

	if (FN && list_tail(&pna->ops, op_t, l)->op != OP_RET
	       && list_tail(&pna->ops, op_t, l)->op != OP_BAIL) {
		op = s_asm_op(pna, OP_RET);
	}
	return 0;

bail:
	return 1;
}
#undef NEXT
#undef ERROR
#undef BADTOKEN

static int s_asm_bytecode(asm_t *pna)
{
	assert(pna);
	/* phases of compilation:

	   I.   insert runtime at addr 0
	   II.  determine offset of each opcode
	   III. resolve labels / relative addresses
	   IV.  pack 'external memory' data
	   V.   encode
	 */

	op_t *op, *tmp;
	int rc;

	/* phase I: runtime insertion */
	op = vmalloc(sizeof(op_t));
	op->op = OP_JMP; /* jmp, don't call */
	op->args[0].type = VALUE_FNLABEL;
	op->args[0]._.label = strdup("main");
	list_unshift(&pna->ops, &op->l);

	/* phase II: calculate offsets & sizes */
	dword_t text = 2; /* 0x7068 (pn) */
	dword_t data = 0;
	/* strings we already saw */
	hash_t seen; memset(&seen, 0, sizeof(seen));
	for_each_object(op, &pna->ops, l) {
		op->offset = text;
		if (op->special) continue;
		if (op->op == OP_ANNO && pna->flags & PNASM_FLAG_STRIP) continue;

		text += 2; /* 2-byte opcode  */

		/* immediate operand values */
		switch (op->args[0].type) {
		case VALUE_NONE:  break;
		case VALUE_EMBED: text += strlen(op->args[0]._.string) + 1; break;
		default:          text += 4; break;
		}

		switch (op->args[1].type) {
		case VALUE_NONE:  break;
		case VALUE_EMBED: text += strlen(op->args[1]._.string) + 1; break;
		default:          text += 4; break;
		}

		/* check for string lengths */
		if (op->args[0].type == VALUE_STRING && !hash_get(&seen, op->args[0]._.string)) {
			data += strlen(op->args[0]._.string) + 1;
			hash_set(&seen, op->args[0]._.string, "Y");
		}
		if (op->args[1].type == VALUE_STRING && !hash_get(&seen, op->args[1]._.string)) {
			data += strlen(op->args[1]._.string) + 1;
			hash_set(&seen, op->args[1]._.string, "Y");
		}
	}
	text += 2; /* OPx_EOF 0x00 */
	hash_done(&seen, 0);

	pna->data.fill = pna->data.offset = text;
	pna->size = text + data;
	pna->code = vcalloc(pna->size, sizeof(byte_t));
	byte_t *c = pna->code; char *p;

	/* HEADER */
	*c++ = 'p'; *c++ = 'n';
	for_each_object(op, &pna->ops, l) {
		if (op->special) continue;
		if (op->op == OP_ANNO && pna->flags & PNASM_FLAG_STRIP) continue;

		/* phase II/III: resolve labels / pack strings */
		rc = s_asm_resolve(pna, &op->args[0], op); if (rc) return rc;
		rc = s_asm_resolve(pna, &op->args[1], op); if (rc) return rc;

		/* phase IV: encode */
		*c++ = op->op;
		*c++ = ((op->args[0].bintype & 0xff) << 4)
			 | ((op->args[1].bintype & 0xff));

		if (op->args[0].type) {
			if (op->args[0].type == VALUE_EMBED) {
				p = op->args[0]._.string;
				while ((*c++ = *p++));
			} else {
				*c++ = ((op->args[0]._.literal >> 24) & 0xff);
				*c++ = ((op->args[0]._.literal >> 16) & 0xff);
				*c++ = ((op->args[0]._.literal >>  8) & 0xff);
				*c++ = ((op->args[0]._.literal >>  0) & 0xff);
			}
		}
		if (op->args[1].type) {
			if (op->args[1].type == VALUE_EMBED) {
				p = op->args[1]._.string;
				while ((*c++ = *p++));
			} else {
				*c++ = ((op->args[1]._.literal >> 24) & 0xff);
				*c++ = ((op->args[1]._.literal >> 16) & 0xff);
				*c++ = ((op->args[1]._.literal >>  8) & 0xff);
				*c++ = ((op->args[1]._.literal >>  0) & 0xff);
			}
		}
	}
	*c++ = OPx_EOF; *c++ = 0x00;

	for_each_object_safe(op, tmp, &pna->ops, l) {
		if (op->special == SPECIAL_FUNC) free(op->label);
		if (op->args[0].type == VALUE_EMBED) free(op->args[0]._.string);
		if (op->args[1].type == VALUE_EMBED) free(op->args[1]._.string);
		free(op);
	}
	hash_done(&pna->data.strings, 0);
	return 0;
}

asm_t *asm_new(void)
{
	asm_t *pna = vmalloc(sizeof(asm_t));
	if (!pna) return NULL;

	list_init(&pna->units);
	list_init(&pna->ops);
	if (s_asm_unit_push(pna) != 0) {
		asm_free(pna);
		return NULL;
	}
	asm_unit_t *u = s_asm_unit(pna);
	if (!u) {
		asm_free(pna);
		return NULL;
	}
	u->name = strdup("MAIN");

	if (asm_setopt(pna, PNASM_OPT_INCLUDE, PENDULUM_INCLUDE, strlen(PENDULUM_INCLUDE)) != 0) {
		asm_free(pna);
		return NULL;
	}

	return pna;
}

void asm_free(asm_t *pna)
{
	if (!pna) return;
	strings_free(pna->include.paths);
	hash_done(&pna->include.seen, 0);
	hash_done(&pna->funcs, 1);

	asm_unit_t *unit, *tmp;
	for_each_object_safe(unit, tmp, &pna->units, l) {
		if (unit->io)
			fclose(unit->io);
		free(unit->name);
		free(unit->file);
		free(unit);
	}

	free(pna->code);
	free(pna);
}

static const char *PNASM_OPT_NAMES[] = {
	"<invalid>",
	"PNASM_OPT_INIO",
	"PNASM_OPT_INFILE",
	"PNASM_OPT_STRIPPED",
	"PNASM_OPT_INCLUDE",
};
static const char* s_asm_optname(int opt)
{
	if (opt < PNASM_OPT_MIN || opt > PNASM_OPT_MAX) opt = 1;
	return PNASM_OPT_NAMES[opt];
}

int asm_setopt(asm_t *pna, int opt, const void *v, size_t len)
{
	asm_unit_t *current;
	char *tmp;

	errno = EINVAL;
	if (!pna) return -1;

	logger(LOG_DEBUG, "asm: setting assembler option %s (%#04x) on %p (v=%p, l=%u)",
		s_asm_optname(opt), opt, pna, v, len);

	switch (opt) {
	case PNASM_OPT_INIO:
		if (len != sizeof(FILE*)) return -1;

		current = s_asm_unit(pna);
		if (!current) return -1;

		current->io = (FILE*)v;
		break;

	case PNASM_OPT_INFILE:
		if (len < 0)  return -1;

		current = s_asm_unit(pna);
		if (!current) return -1;

		free(current->file);
		current->file = vcalloc(len + 1, sizeof(char));
		if (!current->file) return -1;
		memcpy(current->file, (const char *)v, len);

		break;

	case PNASM_OPT_STRIPPED:
		if (len != sizeof(int)) return -1;

		if (*(int*)v) pna->flags = pna->flags |  PNASM_FLAG_STRIP;
		else          pna->flags = pna->flags & ~PNASM_FLAG_STRIP;
		break;

	case PNASM_OPT_INCLUDE:
		if (len <= 0) return -1;

		tmp = vcalloc(len + 1, sizeof(char));
		if (!tmp) return -1;
		memcpy(tmp, (const char *)v, len);

		strings_free(pna->include.paths);
		pna->include.paths = strings_split(tmp, len, ":", SPLIT_NORMAL);
		free(tmp);

		if (!pna->include.paths) return -1;
		break;

	default:
		return -1;
	}

	return 0;
}

int asm_getopt(asm_t *pna, int opt, void *v, size_t *len)
{
	assert(pna);
	errno = EINVAL;
	return -1;
}

int asm_compile(asm_t *pna)
{
	asm_unit_t *u = s_asm_unit(pna);
	assert(u);

	logger(LOG_DEBUG, "asm: beginning assembly");

	if (!u->io && u->file) {
		logger(LOG_DEBUG, "asm: no explicit IO stream given; opening `%s'", u->file);
		u->io = fopen(u->file, "r");
		if (!u->io) {
			logger(LOG_ERR, "%s: %s (error %u)", u->file, strerror(errno), errno);
			return -1;
		}
	}
	if (!u->io) {
		logger(LOG_ERR, "asm: no file or IO stream given to compilation routine; aborting");
		return -1;
	}

	int rc;
	logger(LOG_DEBUG, "asm: beginning parse phase of assembly");
	rc = s_asm_parse(pna);  if (rc != 0) return rc;
	logger(LOG_DEBUG, "asm: beginning bytecoding phase of assembly");
	rc = s_asm_bytecode(pna); if (rc != 0) return rc;

	return 0;
}

