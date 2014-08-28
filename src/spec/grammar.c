/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "src/spec/grammar.y" /* yacc.c:339  */

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

/**

  grammar.y - Reentrant (pure) Bison LALR Parser

  This file defines the productions necessary to interpret
  tokens found by the lexical analyzer, and subsquently build
  a valid abstract syntax tree to describe policy generators.

 */
#include "private.h"
#line 101 "src/spec/grammar.y" /* yacc.c:339  */

#ifdef YYDEBUG
int yydebug = 1;
#endif

#define MANIFEST(ctx) (((spec_parser_context*)ctx)->root)
#define NODE(op,d1,d2) (manifest_new_stree(MANIFEST(ctx), (op), (d1), (d2)))
#define EXPR(t,n1,n2) manifest_new_stree_expr(MANIFEST(ctx), EXPR_ ## t, (n1), (n2))
#define NEGATE(n) manifest_new_stree_expr(MANIFEST(ctx), EXPR_NOT, (n), NULL)

static struct stree* s_regex(struct manifest *m, const char *literal)
{
	char *re, *d, delim, *opts = NULL;
	const char *p;
	int esc = 0;

	d = re = cw_alloc(sizeof(char) * (strlen(literal)+1));
	p = literal;
	if (*p == 'm') p++;
	delim = *p++;

	for (; *p; p++) {
		if (esc) {
			if (*p != delim)
				*d++ = '\\';
			*d++ = *p;
			esc = 0;
			continue;
		}

		if (*p == '\\') {
			esc = 1;
			continue;
		}

		if (*p == delim) {
			opts = strdup(p+1);
			break;
		}

		*d++ = *p;
	}

	if (!opts) opts = strdup("");
	return manifest_new_stree(m, EXPR_REGEX, re, opts);
}


#line 146 "src/spec/grammar.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "grammar.h".  */
#ifndef YY_YY_SRC_SPEC_GRAMMAR_H_INCLUDED
# define YY_YY_SRC_SPEC_GRAMMAR_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    T_KEYWORD_POLICY = 258,
    T_KEYWORD_HOST = 259,
    T_KEYWORD_ENFORCE = 260,
    T_KEYWORD_EXTEND = 261,
    T_KEYWORD_IF = 262,
    T_KEYWORD_UNLESS = 263,
    T_KEYWORD_ELSE = 264,
    T_KEYWORD_MAP = 265,
    T_KEYWORD_DEFAULT = 266,
    T_KEYWORD_AND = 267,
    T_KEYWORD_OR = 268,
    T_KEYWORD_IS = 269,
    T_KEYWORD_NOT = 270,
    T_KEYWORD_LIKE = 271,
    T_KEYWORD_DOUBLE_EQUAL = 272,
    T_KEYWORD_BANG_EQUAL = 273,
    T_KEYWORD_EQUAL_TILDE = 274,
    T_KEYWORD_BANG_TILDE = 275,
    T_KEYWORD_DEPENDS_ON = 276,
    T_KEYWORD_AFFECTS = 277,
    T_KEYWORD_DEFAULTS = 278,
    T_KEYWORD_FALLBACK = 279,
    T_KEYWORD_ALLOW = 280,
    T_KEYWORD_DENY = 281,
    T_KEYWORD_FINAL = 282,
    T_KEYWORD_ALL = 283,
    T_ACLGROUP = 284,
    T_IDENTIFIER = 285,
    T_FACT = 286,
    T_QSTRING = 287,
    T_NUMERIC = 288,
    T_REGEX = 289
  };
#endif
/* Tokens.  */
#define T_KEYWORD_POLICY 258
#define T_KEYWORD_HOST 259
#define T_KEYWORD_ENFORCE 260
#define T_KEYWORD_EXTEND 261
#define T_KEYWORD_IF 262
#define T_KEYWORD_UNLESS 263
#define T_KEYWORD_ELSE 264
#define T_KEYWORD_MAP 265
#define T_KEYWORD_DEFAULT 266
#define T_KEYWORD_AND 267
#define T_KEYWORD_OR 268
#define T_KEYWORD_IS 269
#define T_KEYWORD_NOT 270
#define T_KEYWORD_LIKE 271
#define T_KEYWORD_DOUBLE_EQUAL 272
#define T_KEYWORD_BANG_EQUAL 273
#define T_KEYWORD_EQUAL_TILDE 274
#define T_KEYWORD_BANG_TILDE 275
#define T_KEYWORD_DEPENDS_ON 276
#define T_KEYWORD_AFFECTS 277
#define T_KEYWORD_DEFAULTS 278
#define T_KEYWORD_FALLBACK 279
#define T_KEYWORD_ALLOW 280
#define T_KEYWORD_DENY 281
#define T_KEYWORD_FINAL 282
#define T_KEYWORD_ALL 283
#define T_ACLGROUP 284
#define T_IDENTIFIER 285
#define T_FACT 286
#define T_QSTRING 287
#define T_NUMERIC 288
#define T_REGEX 289

/* Value type.  */



int yyparse (spec_parser_context *ctx);

#endif /* !YY_YY_SRC_SPEC_GRAMMAR_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 259 "src/spec/grammar.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  2
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   206

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  40
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  40
/* YYNRULES -- Number of rules.  */
#define YYNRULES  86
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  170

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   289

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      37,    38,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    39,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    35,     2,    36,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   153,   153,   154,   161,   166,   170,   177,   178,   180,
     184,   188,   193,   201,   202,   204,   208,   215,   216,   220,
     220,   220,   220,   220,   223,   228,   233,   238,   246,   247,
     253,   254,   258,   260,   289,   289,   292,   297,   305,   306,
     308,   312,   313,   314,   315,   316,   319,   319,   321,   321,
     323,   323,   325,   325,   327,   329,   332,   334,   338,   340,
     344,   347,   351,   355,   359,   365,   368,   375,   377,   381,
     381,   383,   390,   390,   394,   395,   402,   406,   413,   414,
     418,   419,   422,   423,   424,   427,   428
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "T_KEYWORD_POLICY", "T_KEYWORD_HOST",
  "T_KEYWORD_ENFORCE", "T_KEYWORD_EXTEND", "T_KEYWORD_IF",
  "T_KEYWORD_UNLESS", "T_KEYWORD_ELSE", "T_KEYWORD_MAP",
  "T_KEYWORD_DEFAULT", "T_KEYWORD_AND", "T_KEYWORD_OR", "T_KEYWORD_IS",
  "T_KEYWORD_NOT", "T_KEYWORD_LIKE", "T_KEYWORD_DOUBLE_EQUAL",
  "T_KEYWORD_BANG_EQUAL", "T_KEYWORD_EQUAL_TILDE", "T_KEYWORD_BANG_TILDE",
  "T_KEYWORD_DEPENDS_ON", "T_KEYWORD_AFFECTS", "T_KEYWORD_DEFAULTS",
  "T_KEYWORD_FALLBACK", "T_KEYWORD_ALLOW", "T_KEYWORD_DENY",
  "T_KEYWORD_FINAL", "T_KEYWORD_ALL", "T_ACLGROUP", "T_IDENTIFIER",
  "T_FACT", "T_QSTRING", "T_NUMERIC", "T_REGEX", "'{'", "'}'", "'('",
  "')'", "':'", "$accept", "manifest", "host", "enforcing", "enforce",
  "conditional_enforce", "alt_condition_enforce", "policy", "blocks",
  "block", "resource", "optional_attributes", "attributes", "attribute",
  "literal_value", "conditional", "alt_condition", "expr", "expr_eq",
  "expr_not_eq", "expr_like", "expr_not_like", "simple_expr", "lvalue",
  "rvalue", "regex", "extension", "dependency", "resource_id", "map",
  "map_conds", "map_rvalue", "map_cond", "map_else", "map_default", "acl",
  "acl_disposition", "acl_subject", "acl_command", "qstring", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   123,   125,    40,    41,    58
};
# endif

#define YYPACT_NINF -106

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-106)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -106,   173,  -106,   104,   133,  -106,  -106,  -106,  -106,   -25,
     -21,    -8,  -106,  -106,  -106,    17,     4,    21,   129,   104,
     -19,    19,  -106,  -106,   123,  -106,  -106,  -106,  -106,  -106,
    -106,    30,  -106,   121,   104,    43,    68,  -106,  -106,  -106,
    -106,    83,  -106,   119,  -106,  -106,   112,   112,   149,    90,
     119,   108,   108,  -106,  -106,     2,  -106,   112,   112,  -106,
    -106,  -106,   112,  -106,   112,  -106,    57,  -106,   152,    75,
    -106,    95,  -106,   127,  -106,  -106,  -106,  -106,  -106,   158,
      86,    88,   -17,    66,   162,   103,   112,   112,   151,   143,
     171,  -106,  -106,  -106,  -106,  -106,   148,   148,   154,   154,
     155,    89,  -106,  -106,   156,   157,   150,  -106,  -106,  -106,
    -106,   162,   162,  -106,  -106,  -106,  -106,  -106,  -106,  -106,
    -106,  -106,  -106,  -106,  -106,  -106,   107,    29,    42,    53,
      74,   159,  -106,  -106,   184,   184,   185,   185,   164,    31,
    -106,  -106,    69,  -106,  -106,   160,  -106,  -106,  -106,  -106,
     165,    67,    78,  -106,  -106,  -106,    98,  -106,  -106,  -106,
    -106,   163,  -106,   166,   161,    90,    90,  -106,  -106,  -106
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,     0,     1,     0,     0,     3,     4,    86,    85,     0,
       0,     0,    17,     7,     7,     0,     0,     0,     0,     0,
       0,     0,    78,    79,     0,    16,    18,    19,    20,    21,
      22,     0,    23,     0,     0,     0,     0,     6,     8,     9,
       5,     0,    35,    28,    34,    62,     0,     0,     0,     0,
      28,     0,     0,    80,    81,     0,    10,     0,     0,    30,
      30,    26,     0,    59,     0,    58,     0,    41,     0,     0,
      30,     0,    24,     0,    63,    64,    82,    84,    83,    76,
       0,     0,     0,     0,    43,     0,     0,     0,     0,    46,
       0,    50,    47,    49,    51,    53,     0,     0,     0,     0,
       0,     0,    65,    77,     0,     0,     0,    27,    31,    29,
      42,    44,    45,    17,    48,    52,    60,    54,    55,    61,
      56,    57,    17,    25,     7,     7,     0,     0,     0,     0,
       0,     0,    32,    33,    38,    38,    13,    13,     0,     0,
      36,    37,     0,    11,    12,     0,    17,    40,     7,    15,
       0,     0,     0,    67,    39,    14,    74,    72,    73,    69,
      70,     0,    68,     0,     0,     0,     0,    66,    71,    75
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -106,  -106,  -106,   -13,  -106,    59,    62,  -106,  -105,  -106,
    -106,   153,    24,  -106,   -18,    65,    71,   -42,  -106,  -106,
    -106,  -106,  -106,   -94,   -93,   -92,  -106,  -106,   131,  -106,
    -106,  -106,  -106,  -106,  -106,  -106,  -106,  -106,  -106,    87
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     5,    16,    38,    39,   143,     6,    15,    26,
      27,    61,    82,   108,    65,    28,   140,    66,    96,    97,
      98,    99,    67,    68,   117,   120,    29,    30,    31,   133,
     156,   161,   162,   163,   164,    32,    33,    55,    79,    44
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      43,    17,   116,   116,   118,    69,    50,   121,   127,    34,
      12,    35,    36,   106,    13,    80,    81,   128,    46,   107,
      84,    18,    85,    19,    20,    21,    34,    14,    35,    36,
      76,    71,    77,    18,    78,    19,    20,    21,    20,    21,
      37,   151,    22,    23,   111,   112,    18,    24,    19,    20,
      21,    51,    52,    25,    22,    23,    47,    40,    34,    24,
      35,    36,   116,   159,   160,   134,   146,    22,    23,    86,
      87,    18,    24,    19,    20,    21,    35,    36,   135,    34,
      57,    35,    36,    34,    83,    35,    36,    86,    87,   136,
       9,    11,    22,    23,   101,    88,   106,    24,    86,    87,
      86,    87,   109,   154,   148,    58,    45,   157,   132,   158,
     137,   129,   130,   100,   155,    86,    87,   131,    59,   106,
       7,    56,     8,    42,   104,   123,   105,    62,     7,    63,
       8,    42,   119,   102,     7,   152,     8,     7,    73,     8,
      42,   110,     7,    63,     8,    42,    48,   168,   169,    64,
      53,    54,    41,     7,    60,     8,    42,    10,   114,     7,
      49,     8,    42,     7,    49,     8,    89,    90,    91,    92,
      93,    94,    95,     2,    86,    87,     3,     4,     7,    63,
       8,    42,    74,    75,    70,   103,   113,   115,   119,   126,
     122,   124,   125,   139,   142,   145,   138,   167,   150,   144,
     153,   149,   165,    72,   147,   166,   141
};

static const yytype_uint8 yycheck[] =
{
      18,    14,    96,    97,    97,    47,    24,    99,   113,     5,
      35,     7,     8,    30,    35,    57,    58,   122,    37,    36,
      62,     4,    64,     6,     7,     8,     5,    35,     7,     8,
      28,    49,    30,     4,    32,     6,     7,     8,     7,     8,
      36,   146,    25,    26,    86,    87,     4,    30,     6,     7,
       8,    21,    22,    36,    25,    26,    37,    36,     5,    30,
       7,     8,   156,   156,   156,    36,    35,    25,    26,    12,
      13,     4,    30,     6,     7,     8,     7,     8,    36,     5,
      37,     7,     8,     5,    60,     7,     8,    12,    13,    36,
       3,     4,    25,    26,    70,    38,    30,    30,    12,    13,
      12,    13,    36,    36,    35,    37,    19,     9,   126,    11,
      36,   124,   125,    38,    36,    12,    13,    10,    35,    30,
      30,    34,    32,    33,    38,    36,    38,    15,    30,    31,
      32,    33,    34,    38,    30,   148,    32,    30,    30,    32,
      33,    38,    30,    31,    32,    33,    23,   165,   166,    37,
      29,    30,    23,    30,    35,    32,    33,    24,    15,    30,
      37,    32,    33,    30,    37,    32,    14,    15,    16,    17,
      18,    19,    20,     0,    12,    13,     3,     4,    30,    31,
      32,    33,    51,    52,    35,    27,    35,    16,    34,    39,
      35,    35,    35,     9,     9,    31,    37,    36,    38,   137,
      35,   142,    39,    50,   139,    39,   135
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    41,     0,     3,     4,    42,    47,    30,    32,    79,
      24,    79,    35,    35,    35,    48,    43,    43,     4,     6,
       7,     8,    25,    26,    30,    36,    49,    50,    55,    66,
      67,    68,    75,    76,     5,     7,     8,    36,    44,    45,
      36,    23,    33,    54,    79,    79,    37,    37,    23,    37,
      54,    21,    22,    29,    30,    77,    79,    37,    37,    35,
      35,    51,    15,    31,    37,    54,    57,    62,    63,    57,
      35,    54,    51,    30,    68,    68,    28,    30,    32,    78,
      57,    57,    52,    52,    57,    57,    12,    13,    38,    14,
      15,    16,    17,    18,    19,    20,    58,    59,    60,    61,
      38,    52,    38,    27,    38,    38,    30,    36,    53,    36,
      38,    57,    57,    35,    15,    16,    63,    64,    64,    34,
      65,    65,    35,    36,    35,    35,    39,    48,    48,    43,
      43,    10,    54,    69,    36,    36,    36,    36,    37,     9,
      56,    56,     9,    46,    46,    31,    35,    55,    35,    45,
      38,    48,    43,    35,    36,    36,    70,     9,    11,    64,
      65,    71,    72,    73,    74,    39,    39,    36,    54,    54
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    40,    41,    41,    41,    42,    42,    43,    43,    43,
      44,    45,    45,    46,    46,    46,    47,    48,    48,    49,
      49,    49,    49,    49,    50,    50,    50,    50,    51,    51,
      52,    52,    53,    53,    54,    54,    55,    55,    56,    56,
      56,    57,    57,    57,    57,    57,    58,    58,    59,    59,
      60,    60,    61,    61,    62,    62,    62,    62,    63,    63,
      64,    65,    66,    67,    67,    68,    69,    70,    70,    71,
      71,    72,    73,    73,    74,    74,    75,    75,    76,    76,
      77,    77,    78,    78,    78,    79,    79
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     2,     2,     5,     5,     0,     2,     2,
       2,     8,     8,     0,     4,     2,     5,     0,     2,     1,
       1,     1,     1,     1,     3,     5,     3,     5,     0,     3,
       0,     2,     3,     3,     1,     1,     8,     8,     0,     4,
       2,     1,     3,     2,     3,     3,     1,     1,     2,     1,
       1,     1,     2,     1,     3,     3,     3,     3,     1,     1,
       1,     1,     2,     3,     3,     4,     8,     0,     2,     1,
       1,     3,     1,     1,     0,     3,     3,     4,     1,     1,
       1,     1,     1,     1,     1,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (ctx, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, ctx); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, spec_parser_context *ctx)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (ctx);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, spec_parser_context *ctx)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, ctx);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, spec_parser_context *ctx)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , ctx);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, ctx); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, spec_parser_context *ctx)
{
  YYUSE (yyvaluep);
  YYUSE (ctx);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (spec_parser_context *ctx)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, YYLEX_PARAM);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 153 "src/spec/grammar.y" /* yacc.c:1646  */
    { MANIFEST(ctx) = manifest_new(); }
#line 1470 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 3:
#line 155 "src/spec/grammar.y" /* yacc.c:1646  */
    { stree_add(MANIFEST(ctx)->root, (yyvsp[0].stree));
		  if ((yyvsp[0].stree)->data1) {
			cw_hash_set(MANIFEST(ctx)->hosts, (yyvsp[0].stree)->data1, (yyvsp[0].stree));
		  } else {
			MANIFEST(ctx)->fallback = (yyvsp[0].stree);
		  } }
#line 1481 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 4:
#line 162 "src/spec/grammar.y" /* yacc.c:1646  */
    { stree_add(MANIFEST(ctx)->root, (yyvsp[0].stree));
		  cw_hash_set(MANIFEST(ctx)->policies, (yyvsp[0].stree)->data1, (yyvsp[0].stree)); }
#line 1488 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 5:
#line 167 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[-1].stree);
		  (yyval.stree)->op = HOST;
		  (yyval.stree)->data1 = (yyvsp[-3].string); }
#line 1496 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 6:
#line 171 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[-1].stree);
		  (yyval.stree)->op = HOST;
		  (yyval.stree)->data1 = NULL; }
#line 1504 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 7:
#line 177 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(PROG, NULL, NULL); }
#line 1510 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 8:
#line 179 "src/spec/grammar.y" /* yacc.c:1646  */
    { stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1516 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 9:
#line 181 "src/spec/grammar.y" /* yacc.c:1646  */
    { stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1522 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 10:
#line 185 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(INCLUDE, (yyvsp[0].string), NULL); }
#line 1528 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 11:
#line 189 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(IF, NULL, NULL);
		  stree_add((yyval.stree), (yyvsp[-5].stree));
		  stree_add((yyval.stree), (yyvsp[-2].stree));
		  stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1537 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 12:
#line 194 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(IF, NULL, NULL);
		  stree_add((yyval.stree), NEGATE((yyvsp[-5].stree)));
		  stree_add((yyval.stree), (yyvsp[-2].stree));
		  stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1546 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 13:
#line 201 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(NOOP, NULL, NULL); }
#line 1552 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 14:
#line 203 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[-1].stree); }
#line 1558 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 15:
#line 205 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[0].stree); }
#line 1564 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 16:
#line 209 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[-1].stree);
		  (yyval.stree)->op = POLICY;
		  (yyval.stree)->data1 = (yyvsp[-3].string); }
#line 1572 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 17:
#line 215 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(PROG, NULL, NULL); }
#line 1578 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 18:
#line 217 "src/spec/grammar.y" /* yacc.c:1646  */
    { stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1584 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 24:
#line 224 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[0].stree);
		  (yyval.stree)->op = RESOURCE;
		  (yyval.stree)->data1 = (yyvsp[-2].string);
		  (yyval.stree)->data2 = (yyvsp[-1].string); }
#line 1593 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 25:
#line 229 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[-1].stree);
		  (yyval.stree)->op = RESOURCE;
		  (yyval.stree)->data1 = (yyvsp[-4].string);
		  (yyval.stree)->data2 = NULL; }
#line 1602 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 26:
#line 234 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[0].stree);
		  (yyval.stree)->op = RESOURCE;
		  (yyval.stree)->data1 = cw_strdup("host"); /* dynamic string for stree_free */
		  (yyval.stree)->data2 = (yyvsp[-1].string); }
#line 1611 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 27:
#line 239 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[-1].stree);
		  (yyval.stree)->op = RESOURCE;
		  (yyval.stree)->data1 = cw_strdup("host"); /* dynamic string for stree_free */
		  (yyval.stree)->data2 = NULL; }
#line 1620 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 28:
#line 246 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(PROG, NULL, NULL); }
#line 1626 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 29:
#line 248 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[-1].stree); }
#line 1632 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 30:
#line 253 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(PROG, NULL, NULL); }
#line 1638 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 31:
#line 255 "src/spec/grammar.y" /* yacc.c:1646  */
    { stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1644 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 32:
#line 259 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(ATTR, (yyvsp[-2].string), (yyvsp[0].string)); }
#line 1650 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 33:
#line 261 "src/spec/grammar.y" /* yacc.c:1646  */
    {
			struct stree *n;
			parser_map_cond *c, *tmp;
			(yyval.stree) = NULL;

			for_each_object_safe_r(c, tmp, &(yyvsp[0].map)->cond, l) {
				if (c->rhs) {
					if (c->rhs) {
						n = NODE(IF, NULL, NULL);
						stree_add(n, c->rhs->op == EXPR_REGEX
								? EXPR(MATCH, (yyvsp[0].map)->lhs, c->rhs)
								: EXPR(EQ,    (yyvsp[0].map)->lhs, c->rhs));
						stree_add(n, NODE(ATTR, strdup((yyvsp[-2].string)), c->value));
					} else {
						n = NODE(ATTR, strdup((yyvsp[-2].string)), c->value);
					}
					if ((yyval.stree)) stree_add(n, (yyval.stree));
					(yyval.stree) = n;
				} else {
					(yyval.stree) = NODE(ATTR, strdup((yyvsp[-2].string)), c->value);
				}
				free(c);
			}
			free((yyvsp[-2].string));
			free((yyvsp[0].map));
		}
#line 1681 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 36:
#line 293 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(IF, NULL, NULL);
		  stree_add((yyval.stree), (yyvsp[-5].stree));
		  stree_add((yyval.stree), (yyvsp[-2].stree));
		  stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1690 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 37:
#line 298 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(IF, NULL, NULL);
		  stree_add((yyval.stree), NEGATE((yyvsp[-5].stree)));
		  stree_add((yyval.stree), (yyvsp[-2].stree));
		  stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1699 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 38:
#line 305 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(NOOP, NULL, NULL); }
#line 1705 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 39:
#line 307 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[-1].stree); }
#line 1711 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 40:
#line 309 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = (yyvsp[0].stree); }
#line 1717 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 42:
#line 313 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = EXPR(NOOP, (yyvsp[-1].stree), NULL); }
#line 1723 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 43:
#line 314 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NEGATE((yyvsp[0].stree)); }
#line 1729 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 44:
#line 315 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = EXPR(AND, (yyvsp[-2].stree), (yyvsp[0].stree)); }
#line 1735 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 45:
#line 316 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = EXPR(OR,  (yyvsp[-2].stree), (yyvsp[0].stree)); }
#line 1741 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 54:
#line 328 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = EXPR(EQ, (yyvsp[-2].stree), (yyvsp[0].stree)); }
#line 1747 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 55:
#line 330 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NEGATE(EXPR(EQ, (yyvsp[-2].stree), (yyvsp[0].stree))); }
#line 1753 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 56:
#line 333 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = EXPR(MATCH, (yyvsp[-2].stree), (yyvsp[0].stree)); }
#line 1759 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 57:
#line 335 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NEGATE(EXPR(MATCH, (yyvsp[-2].stree), (yyvsp[0].stree))); }
#line 1765 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 58:
#line 339 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(EXPR_VAL, (yyvsp[0].string), NULL); }
#line 1771 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 59:
#line 341 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(EXPR_FACT, (yyvsp[0].string), NULL); }
#line 1777 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 61:
#line 348 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = s_regex(MANIFEST(ctx), (yyvsp[0].string)); free((yyvsp[0].string)); }
#line 1783 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 62:
#line 352 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(INCLUDE, (yyvsp[0].string), NULL); }
#line 1789 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 63:
#line 356 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(DEPENDENCY, NULL, NULL);
		  stree_add((yyval.stree), (yyvsp[-2].stree));
		  stree_add((yyval.stree), (yyvsp[0].stree)); }
#line 1797 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 64:
#line 360 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(DEPENDENCY, NULL, NULL);
		  stree_add((yyval.stree), (yyvsp[0].stree));
		  stree_add((yyval.stree), (yyvsp[-2].stree)); }
#line 1805 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 65:
#line 366 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(RESOURCE_ID, (yyvsp[-3].string), (yyvsp[-1].string)); }
#line 1811 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 66:
#line 369 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.map) = (yyvsp[-2].map);
		  if ((yyvsp[-1].map_cond)) cw_list_push(&(yyval.map)->cond, &(yyvsp[-1].map_cond)->l);
		  (yyvsp[-2].map)->lhs = NODE(EXPR_FACT, (yyvsp[-5].string), NULL); }
#line 1819 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 67:
#line 375 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.map) = cw_alloc(sizeof(parser_map));
		  cw_list_init(&(yyval.map)->cond); }
#line 1826 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 68:
#line 378 "src/spec/grammar.y" /* yacc.c:1646  */
    { cw_list_push(&(yyval.map)->cond, &(yyvsp[0].map_cond)->l); }
#line 1832 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 71:
#line 384 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.map_cond) = cw_alloc(sizeof(parser_map_cond));
		  cw_list_init(&(yyval.map_cond)->l);
		  (yyval.map_cond)->rhs   = (yyvsp[-2].stree);
		  (yyval.map_cond)->value = (yyvsp[0].string); }
#line 1841 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 74:
#line 394 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.map_cond) = NULL; }
#line 1847 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 75:
#line 396 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.map_cond) = cw_alloc(sizeof(parser_map_cond));
		  cw_list_init(&(yyval.map_cond)->l);
		  (yyval.map_cond)->value = (yyvsp[0].string); }
#line 1855 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 76:
#line 403 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(ACL, (yyvsp[-2].string), strdup("continue"));
		  stree_add((yyval.stree), (yyvsp[-1].stree));
		  stree_add((yyval.stree), NODE(ACL_COMMAND, (yyvsp[0].string), NULL)); }
#line 1863 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 77:
#line 407 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(ACL, (yyvsp[-3].string), strdup("final"));
		  stree_add((yyval.stree), (yyvsp[-2].stree));
		  stree_add((yyval.stree), NODE(ACL_COMMAND, (yyvsp[-1].string), NULL)); }
#line 1871 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 78:
#line 413 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.string) = strdup("allow"); }
#line 1877 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 79:
#line 414 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.string) = strdup("deny");  }
#line 1883 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 80:
#line 418 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(ACL_SUBJECT, NULL, (yyvsp[0].string)); }
#line 1889 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 81:
#line 419 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.stree) = NODE(ACL_SUBJECT, (yyvsp[0].string), NULL); }
#line 1895 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 82:
#line 422 "src/spec/grammar.y" /* yacc.c:1646  */
    { (yyval.string) = strdup("*"); }
#line 1901 "src/spec/grammar.c" /* yacc.c:1646  */
    break;

  case 86:
#line 429 "src/spec/grammar.y" /* yacc.c:1646  */
    { spec_parser_warning(YYPARSE_PARAM, "unexpected identifier '%s', expected quoted string literal", (yyvsp[0].string)); }
#line 1907 "src/spec/grammar.c" /* yacc.c:1646  */
    break;


#line 1911 "src/spec/grammar.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (ctx, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (ctx, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, ctx);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, ctx);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (ctx, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, ctx);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, ctx);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
