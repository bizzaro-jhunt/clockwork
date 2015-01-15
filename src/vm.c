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

#define LINE_BUF_SIZE 8192
typedef struct {
	FILE       *io;
	const char *file;
	int         line;
	int         token;
	char        value[LINE_BUF_SIZE];
	char        buffer[LINE_BUF_SIZE];
	char        raw[LINE_BUF_SIZE];

	list_t      ops;

	hash_t      strings;
	dword_t     static_offset;
	dword_t     static_fill;

	byte_t     *code;
	size_t      size;
} compiler_t;

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

static int s_asm_lex(compiler_t *cc)
{
	assert(cc);

	char *a, *b;
	if (!*cc->buffer || *cc->buffer == ';') {
getline:
		if (!fgets(cc->raw, LINE_BUF_SIZE, cc->io))
			return 0;
		cc->line++;

		a = cc->raw;
		while (*a && isspace(*a)) a++;
		if (*a == ';')
			while (*a && *a != '\n') a++;
		while (*a && isspace(*a)) a++;
		if (!*a)
			goto getline;

		b = cc->buffer;
		while ((*b++ = *a++));
	}
	cc->token = 0;
	cc->value[0] = '\0';

	b = cc->buffer;
	while (*b && isspace(*b)) b++;
	a = b;

#define SHIFTLINE do { \
	while (*b && isspace(*b)) b++; \
	memmove(cc->buffer, b, LINE_BUF_SIZE - (b - cc->buffer)); \
} while (0)

	if (*b == '%') { /* register */
		while (*b && !isspace(*b)) b++;
		if (!*b || isspace(*b)) {
			*b = '\0';
			char reg = b - a - 2 ? '0' : *(a+1);
			if (reg < 'a' || reg >= 'a'+NREGS) {
				logger(LOG_ERR, "%s:%i: unrecognized register address %s (%i)", cc->file, cc->line, a, reg);
				return 0;
			}

			cc->token = T_REGISTER;
			cc->value[0] = reg;
			cc->value[1] = '\0';

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

			cc->token = T_OFFSET;
			memcpy(cc->value, cc->buffer, b-cc->buffer);

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

			cc->token = T_NUMBER;
			memcpy(cc->value, cc->buffer, b-cc->buffer);

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
			cc->token = T_LABEL;
			memcpy(cc->value, cc->buffer, b-cc->buffer);
			SHIFTLINE;
			return 1;
		}
		b = a;

		while (*b && (isalnum(*b) || *b == '.' || *b == '_' || *b == '?' || *b == '-' || *b == ':'))
			b++;
		*b++ = '\0';

		if (strcmp(a, "acl") == 0) {
			cc->token = T_ACL;
			a = b;
			while (*b && *b != '\n') b++;
			*b = '\0';
			memcpy(cc->value, a, b - a + 1);
			memset(cc->buffer, 0, LINE_BUF_SIZE);
			return 1;
		}

		int i;
		for (i = 0; ASM[i]; i++) {
			if (strcmp(a, ASM[i]) != 0) continue;
			cc->token = T_OPCODE;
			cc->value[0] = T_OP_NOOP + i;
			cc->value[1] = '\0';
			break;
		}

		if (strcmp(a, "fn") == 0) cc->token = T_FUNCTION;

		if (!cc->token) {
			cc->token = T_IDENTIFIER;
			memcpy(cc->value, cc->buffer, b-cc->buffer);
		}

		SHIFTLINE;
		return 1;
	}

	if (*b == '"') {
		b++; a = cc->value;
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
		else logger(LOG_WARNING, "%s:%i: unterminated string literal", cc->file, cc->line);

		cc->token = T_STRING;
		SHIFTLINE;
		return 1;
	}


	logger(LOG_ERR, "%s:%i: failed to parse '%s'", cc->file, cc->line, cc->buffer);
	return 0;
}

static int s_asm_parse(compiler_t *cc)
{
	assert(cc);

	op_t *FN = NULL;
	list_init(&cc->ops);

#define NEXT if (!s_asm_lex(cc)) { logger(LOG_CRIT, "%s:%i: unexpected end of configuration\n", cc->file, cc->line); goto bail; }
#define ERROR(s) do { logger(LOG_CRIT, "%s:%i: syntax error: %s", cc->file, cc->line, s); goto bail; } while (0)
#define BADTOKEN(s) do { logger(LOG_CRIT, "%s:%i: unexpected " s " '%s'", cc->file, cc->line, cc->value); goto bail; } while (0)

	int i, j;
	op_t *op;
	while (s_asm_lex(cc)) {
		op = calloc(1, sizeof(op_t));
		op->fn = FN;

		switch (cc->token) {
		case T_FUNCTION:
			if (FN && list_tail(&cc->ops, op_t, l)->op != OP_RET) {
				op->op = OP_RET;
				list_push(&cc->ops, &op->l);
				op = calloc(1, sizeof(op_t));
			}
			FN = op;
			op->special = SPECIAL_FUNC;
			NEXT;
			if (cc->token != T_IDENTIFIER)
				ERROR("unacceptable name for function");
			op->label = strdup(cc->value);
			break;

		case T_ACL:
			op->op = OP_ACL;
			op->args[0].type = VALUE_STRING;
			op->args[0]._.string = strdup(cc->value);
			break;

		case T_LABEL:
			op->special = SPECIAL_LABEL;
			op->label = strdup(cc->value);
			break;

		case T_OPCODE:
			for (i = 0; ASM_SYNTAX[i].token; i++) {
				if ((byte_t)cc->value[0] != ASM_SYNTAX[i].token) continue;
				op->op = (byte_t)ASM_SYNTAX[i].opcode;

				for (j = 0; j < 2; j++) {
					if (ASM_SYNTAX[i].args[j] == ARG_NONE) break;
					NEXT;

					if (cc->token == T_REGISTER && ASM_SYNTAX[i].args[j] & ARG_REGISTER) {
						op->args[j].type = VALUE_REGISTER;
						op->args[j]._.regname = cc->value[0];

					} else if (cc->token == T_NUMBER && ASM_SYNTAX[i].args[j] & ARG_NUMBER) {
						op->args[j].type = VALUE_NUMBER;
						op->args[j]._.literal = strtol(cc->value, NULL, 0);

					} else if (cc->token == T_STRING && ASM_SYNTAX[i].args[j] & ARG_STRING) {
						op->args[j].type = VALUE_STRING;
						op->args[j]._.string = strdup(cc->value);

					} else if (cc->token == T_IDENTIFIER && ASM_SYNTAX[i].args[j] & ARG_LABEL) {
						op->args[j].type = VALUE_LABEL;
						op->args[j]._.label = strdup(cc->value);

					} else if (cc->token == T_IDENTIFIER && ASM_SYNTAX[i].args[j] & ARG_FUNCTION) {
						op->args[j].type = VALUE_FNLABEL;
						op->args[j]._.fnlabel = strdup(cc->value);

					} else if (cc->token == T_IDENTIFIER && ASM_SYNTAX[i].args[j] & ARG_IDENTIFIER) {
						op->args[j].type = VALUE_STRING;
						op->args[j]._.string = strdup(cc->value);

					} else if (cc->token == T_OFFSET && ASM_SYNTAX[i].args[j] & ARG_LABEL) {
						op->args[j].type = VALUE_OFFSET;
						op->args[j]._.offset = strtol(cc->value, NULL, 10);

					} else {
						logger(LOG_CRIT, "%s: %i: invalid form; expected `%s`",
							cc->file, cc->line, ASM_SYNTAX[i].usage);
						goto bail;
					}
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

		list_push(&cc->ops, &op->l);
	}

	if (FN && list_tail(&cc->ops, op_t, l)->op != OP_RET) {
		op = calloc(1, sizeof(op_t));
		op->op = OP_RET;
		list_push(&cc->ops, &op->l);
	}
	return 0;

bail:
	return 1;
}

static int s_asm_resolve(compiler_t *cc, value_t *v, op_t *me)
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
		addr = hash_get(&cc->strings, v->_.string);
		if (!addr) {
			len = strlen(v->_.string) + 1;
			memcpy(cc->code + cc->static_fill, v->_.string, len);

			addr = cc->code + cc->static_fill;
			hash_set(&cc->strings, v->_.string, addr);
			cc->static_fill += len;
		}
		free(v->_.string);

		v->type = VALUE_ADDRESS;
		v->bintype = TYPE_ADDRESS;
		v->_.address = addr - cc->code;
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
		logger(LOG_ERR, "label %s not found in scope!", v->_.label);
		return 1;

	case VALUE_FNLABEL:
		for_each_object(op, &me->l, l) {
			if (op->special != SPECIAL_FUNC) continue;
			if (strcmp(v->_.fnlabel, op->label) != 0) continue;

			free(v->_.fnlabel);
			v->type = VALUE_ADDRESS;
			v->bintype = TYPE_ADDRESS;
			v->_.address = op->offset;
			return 0;
		}
		logger(LOG_ERR, "fnlabel %s not found globally!", v->_.fnlabel);
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

static int s_asm_encode(compiler_t *cc)
{
	assert(cc);
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
	op = calloc(1, sizeof(op_t));
	op->op = OP_JMP; /* jmp, don't call */
	op->args[0].type = VALUE_FNLABEL;
	op->args[0]._.label = strdup("main");
	list_unshift(&cc->ops, &op->l);

	/* phase II: calculate offsets & sizes */
	dword_t text = 2; /* 0x7068 (pn) */
	dword_t data = 0;
	for_each_object(op, &cc->ops, l) {
		op->offset = text;
		if (op->special) continue;
		text += 2;                                     /* 2-byte opcode  */
		if (op->args[0].type != VALUE_NONE) text += 4; /* 4-byte operand */
		if (op->args[1].type != VALUE_NONE) text += 4; /* 4-byte operand */

		/* check for string lengths */
		if (op->args[0].type == VALUE_STRING) data += strlen(op->args[0]._.string) + 1;
		if (op->args[1].type == VALUE_STRING) data += strlen(op->args[1]._.string) + 1;
	}
	text += 2; /* 0xff00 */

	cc->static_fill = cc->static_offset = text;
	cc->size = text + data;
	cc->code = calloc(cc->size, sizeof(byte_t));
	byte_t *c = cc->code;

	/* HEADER */
	*c++ = 'p'; *c++ = 'n';
	for_each_object(op, &cc->ops, l) {
		if (op->special) continue;

		/* phase II/III: resolve labels / pack strings */
		rc = s_asm_resolve(cc, &op->args[0], op); if (rc) return rc;
		rc = s_asm_resolve(cc, &op->args[1], op); if (rc) return rc;

		/* phase IV: encode */
		*c++ = op->op;
		*c++ = ((op->args[0].bintype & 0xff) << 4)
			 | ((op->args[1].bintype & 0xff));

		if (op->args[0].type) {
			*c++ = ((op->args[0]._.literal >> 24) & 0xff);
			*c++ = ((op->args[0]._.literal >> 16) & 0xff);
			*c++ = ((op->args[0]._.literal >>  8) & 0xff);
			*c++ = ((op->args[0]._.literal >>  0) & 0xff);
		}
		if (op->args[1].type) {
			*c++ = ((op->args[1]._.literal >> 24) & 0xff);
			*c++ = ((op->args[1]._.literal >> 16) & 0xff);
			*c++ = ((op->args[1]._.literal >>  8) & 0xff);
			*c++ = ((op->args[1]._.literal >>  0) & 0xff);
		}
	}
	*c++ = 0xff; *c++ = 0x00;

	for_each_object_safe(op, tmp, &cc->ops, l) {
		if (op->special == SPECIAL_FUNC) free(op->label);
		free(op);
	}
	hash_done(&cc->strings, 0);
	return 0;
}

static int s_vm_asm(compiler_t *cc, byte_t **code, size_t *len)
{
	int rc;
	rc = s_asm_parse(cc);  if (rc) return rc;
	rc = s_asm_encode(cc); if (rc) return rc;

	*code = cc->code;
	*len  = cc->size;
	return 0;
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

static dword_t vm_heap_string(vm_t *vm, char *s)
{
	heap_t *h = calloc(1, sizeof(heap_t));
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
	if (!src) return 1;

	dst = open(to, O_WRONLY|O_CREAT|O_TRUNC, 0777);
	if (!dst) {
		close(src);
		return 1;
	}

	size_t n;
	char buf[8192];
	while ((n = read(src, buf, 8192)) > 0) {
		if (write(src, buf, n) == n) continue;
		close(src); close(dst);
		return 1;
	}

	close(src); close(dst);
	return n;
}

static char * s_fs_get(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) return NULL;

	long size = lseek(fd, 0, SEEK_END);
	if (lseek(fd, 0, SEEK_SET) != 0) {
		close(fd);
		return NULL;
	}

	char *s = vmalloc(size * sizeof(char) + 1);
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

	return s;
}

static int s_fs_put(const char *path, const char *contents)
{
	int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0777);
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
	REGISTER1("push");

	s_push(vm, &vm->dstack, REG1(vm));
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
	vm->acc = close(open(STR1(vm), O_CREAT, 0777));
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

static void op_augeas_init(vm_t *vm)
{
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
	vm_fprintf(vm, vm->stderr, STR1(vm));
	s_aug_perror(vm->stderr, vm->aux.augeas);
}

static void op_augeas_write(vm_t *vm)
{
	ARG0("augeas.write");
	vm->acc = aug_save(vm->aux.augeas);
}

static void op_augeas_set(vm_t *vm)
{
	ARG2("augeas.set");
	vm->acc = aug_set(vm->aux.augeas, STR1(vm), STR2(vm));
}

static void op_augeas_get(vm_t *vm)
{
	ARG2("augeas.get");
	REGISTER2("augeas.get");
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
	char *s = s_aug_find(vm->aux.augeas, STR1(vm));
	if (s) {
		vm->acc = 0;
		REG2(vm) = vm_heap_string(vm, s);
	} else {
		vm->acc = 1;
	}
}

static void op_augeas_remove(vm_t *vm)
{
	ARG1("augeas.remove");
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

	char execline[8192];
	runner_t runner = {
		.in  = NULL,
		.out = tmpfile(),
		.err = tmpfile(),
		.uid = geteuid(),
		.gid = getegid(),
	};

	char *s = string("%s %s",
		hash_get(&vm->pragma, "localsys.cmd"),
		_sprintf(vm, STR1(vm))); /* FIXME: mem leak */
	vm->acc = run2(&runner, "/bin/sh", "-c", s, NULL); free(s);

	if (fgets(execline, sizeof(execline), runner.out)) {
		s = strchr(execline, '\n'); if (s) *s = '\0';
		REG2(vm) = vm_heap_strdup(vm, execline);
	}
	fclose(runner.out); runner.out = NULL;
	fclose(runner.err); runner.err = NULL;
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
	abort();
	fprintf(vm->stdout, "looking up property(%s)\n", STR1(vm));
	const char *v = hash_get(&vm->props, STR1(vm));
	if (v) {
		fprintf(vm->stdout, "found property(%s) = '%s'\n", STR1(vm), v);
		REG2(vm) = vm_heap_strdup(vm, v);
		vm->acc = 0;
	} else {
		fprintf(vm->stdout, "could not find property(%s)\n", STR1(vm));
		vm->acc = 1;
	}
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
		     code++; /* op */
		f = *code++; /* f1/f2 */
		if (HI_NYBLE(f)) code += 4;
		if (LO_NYBLE(f)) code += 4;

		if (*code == 0xff) {
			vm->static0 = len - (code - vm->code);
			break;
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
		vm->op = vm->code[vm->pc++];
		vm->f1 = HI_NYBLE(vm->code[vm->pc]);
		vm->f2 = LO_NYBLE(vm->code[vm->pc]);

		if (vm->trace)
			fprintf(vm->stderr, "+%s [%02x]", OPCODES[vm->op], vm->code[vm->pc]);
		vm->pc++;

		if (vm->f2 && !vm->f1)
			B_ERR("corrupt operands mask detected; vm->f1=%02x, vm->f2=%02x", vm->f1, vm->f2);

		if (vm->f1) {
			vm->oper1 = DWORD(vm->code[vm->pc + 0],
			                  vm->code[vm->pc + 1],
			                  vm->code[vm->pc + 2],
			                  vm->code[vm->pc + 3]);
			vm->pc += 4;
			if (vm->trace)
				fprintf(vm->stderr, " %08x", vm->oper1);
		}

		if (vm->f2) {
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

int vm_asm_file(const char *path, byte_t **code, size_t *len)
{
	int rc;
	compiler_t cc;
	memset(&cc, 0, sizeof(cc));

	if (strcmp(path, "-") == 0) {
		cc.file = "<stdin>";
		cc.io   = stdin;
		rc = s_vm_asm(&cc, code, len);

	} else {
		cc.file = path;
		cc.io   = fopen(path, "r");
		rc = s_vm_asm(&cc, code, len);
		fclose(cc.io);
	}

	return rc;
}

int vm_asm_io(FILE *io, byte_t **code, size_t *len)
{
	compiler_t cc;
	memset(&cc, 0, sizeof(cc));

	cc.file = "<unknown>";
	cc.io   = io;
	return s_vm_asm(&cc, code, len);
}

int vm_disasm(vm_t *vm)
{
	if (vm->codesize < 3) return 1;
	if (vm->code[0] != 'p') return 2;
	if (vm->code[1] != 'n') return 2;

	fprintf(stderr, "0x%08x: ", 0);
	fprintf(stderr, "%02x %02x\n", 'p', 'n');

	size_t i = 2;
	while (i < vm->codesize) {
		fprintf(stderr, "0x%08x: ", (dword_t)i);
		vm->op = vm->code[i++];
		if (vm->op == 0xff && !vm->code[i]) {
			fprintf(stderr, "%02x %02x\n", 0xff, 0x00);
			i++;
			break;
		}
		vm->f1 = HI_NYBLE(vm->code[i]);
		vm->f2 = LO_NYBLE(vm->code[i]);
		fprintf(stderr, "%02x %02x", vm->op, vm->code[i++]);

		if (vm->f2 && !vm->f1) {
			fprintf(stderr, "pendulum bytecode error: corrupt operands mask detected; vm->f1=%02x, vm->f2=%02x\n", vm->f1, vm->f2);
			return 1;
		}
		if (vm->f1) {
			fprintf(stderr, " [%02x %02x %02x %02x]",
				vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3]);
			vm->oper1 = DWORD(vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3]);
			i += 4;
		} else {
			fprintf(stderr, "%14s", " ");
		}
		fprintf(stderr, "  %12s", OPCODES[vm->op]);
		if (vm->f1) {
			switch (vm->f1) {
			case TYPE_LITERAL:  fprintf(stderr, " %u",     vm->oper1); break;
			case TYPE_REGISTER: fprintf(stderr, " %%%c",   vm->oper1 + 'a'); break;
			case TYPE_ADDRESS:  fprintf(stderr, " 0x%08x", vm->oper1);
			                    if (vm->oper1 > vm->static0) {
			                        fprintf(stderr, " ; \"");
			                        s_eprintf(stderr, (char *)vm->code + vm->oper1);
			                        fprintf(stderr, "\"");
			                    }
			                    break;
			default:            fprintf(stderr, " ; operand 1 unknown (0x%x/0x%x)",
			                                    vm->f1, vm->oper1);
			}

		}
		if (vm->f2) {
			fprintf(stderr, "\n                  [%02x %02x %02x %02x]  %12s",
				vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3], "");
			vm->oper2 = DWORD(vm->code[i + 0], vm->code[i + 1], vm->code[i + 2], vm->code[i + 3]);
			i += 4;

			switch (vm->f2) {
			case TYPE_LITERAL:  fprintf(stderr, " %u",     vm->oper2); break;
			case TYPE_REGISTER: fprintf(stderr, " %%%c",   vm->oper2 + 'a'); break;
			case TYPE_ADDRESS:  fprintf(stderr, " 0x%08x", vm->oper2);
			                    if (vm->oper2 > vm->static0) {
			                        fprintf(stderr, " ; \"");
			                        s_eprintf(stderr, (char *)vm->code + vm->oper2);
			                        fprintf(stderr, "\"");
			                    }
			                    break;
			default:            fprintf(stderr, " ; operand 2 unknown (0x%x/0x%x)",
			                                    vm->f2, vm->oper2);
			}
		}

		fprintf(stderr, "\n");
	}

	fprintf(stderr, "---\n");
	while (i < vm->codesize) {
		fprintf(stderr, "0x%08x: [", (dword_t)i);
		s_eprintf(stderr, (char *)(vm->code + i));
		fprintf(stderr, "]\n");
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
