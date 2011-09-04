/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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

#ifndef TEST_H
#define TEST_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../local.h"

typedef void (*test_suite_f)(void);

struct test_suite {
	const char    *name;
	test_suite_f   runner;
	int            active;
};

int add_test_suite(const char *name, test_suite_f runner, int active);
#define TEST_SUITE(x) extern void test_suite_ ## x(); add_test_suite(#x, test_suite_ ## x, 0)
void teardown_test_suites(void);

int activate_test(const char *name);
int run_active_tests(void);
int run_all_tests(void);

void test(const char *s);
int test_status(void);
int test_setup(int argc, char **argv);

/** ASSERTIONS **/

void assert_fail(const char *s);
void assert_pass(const char *s);
void assert_true(const char *s, int value);
void assert_false(const char *s, int value);
void assert_not_null(const char *s, const void *ptr);
void assert_null(const char *s, const void *ptr);

void assert_unsigned_eq(const char *s, unsigned long int expected, unsigned long int actual);
void assert_signed_eq(const char *s, signed long int expected, signed long int actual);

void assert_ptr_eq(const char *s, const void *expected, const void *actual);
void assert_ptr_ne(const char *s, const void *unexpected, const void *actual);

void assert_int_eq(const char *s, int expected, int actual);
void assert_int_ne(const char *s, int unexpected, int actual);
void assert_int_gt(const char *s, int actual, int threshold);
void assert_int_ge(const char *s, int actual, int threshold);
void assert_int_lt(const char *s, int actual, int threshold);
void assert_int_le(const char *s, int actual, int threshold);

void assert_str_eq(const char *s, const char *expected, const char *actual);
void assert_str_ne(const char *s, const char *unexpected, const char *actual);

#endif /* TEST_H */
