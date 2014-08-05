/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

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
    T_IDENTIFIER = 280,
    T_FACT = 281,
    T_QSTRING = 282,
    T_NUMERIC = 283,
    T_REGEX = 284
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
#define T_IDENTIFIER 280
#define T_FACT 281
#define T_QSTRING 282
#define T_NUMERIC 283
#define T_REGEX 284

/* Value type.  */



int yyparse (spec_parser_context *ctx);

#endif /* !YY_YY_SRC_SPEC_GRAMMAR_H_INCLUDED  */
