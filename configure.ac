#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.68)
AC_INIT([Clockwork], [2.0.2], [bugs@niftylogic.com])

AC_CONFIG_SRCDIR([src/cw.h])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AX_TEST_COVERAGE

# Checks for programs.
AC_PROG_YACC
AC_PROG_CC
AC_PROG_LEX
AC_PROG_INSTALL

AC_CHECK_LIB(pthread,
	pthread_create,,
	AC_MSG_ERROR(Missing pthread library))

AC_CHECK_LIB(readline,
	read_history,,
	AC_MSG_ERROR(Missing GNU readline library))

AC_CHECK_LIB(augeas,
	aug_init,,
	AC_MSG_ERROR(Missing Augeas library))

AC_HAVE_LIBRARY(ctap,,
	AC_MSG_ERROR(Missing ctap testing library))

AC_CHECK_HEADERS([\
	arpa/inet.h \
	assert.h \
	augeas.h \
	ctype.h \
	errno.h \
	fcntl.h \
	fts.h \
	getopt.h \
	glob.h \
	grp.h \
	libgen.h \
	linux/limits.h \
	netdb.h \
	pthread.h \
	pwd.h \
	readline/history.h \
	readline/readline.h \
	shadow.h \
	signal.h \
	stdarg.h \
	stdint.h \
	stdio.h \
	stdlib.h \
	string.h \
	sys/select.h \
	sys/socket.h \
	sys/stat.h \
	sys/time.h \
	sys/types.h \
	sys/utsname.h \
	sys/wait.h \
	time.h \
	unistd.h \
])

#AC_CHECK_HEADERS([libxml/tree.h])

# Checks for typedefs, structures, and compiler characteristics.

AC_C_INLINE

AC_TYPE_INT64_T
AC_TYPE_INT32_T
AC_TYPE_INT16_T
AC_TYPE_INT8_T

AC_TYPE_MODE_T
AC_TYPE_PID_T

AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_TYPE_UINT64_T
AC_TYPE_UINT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

AC_TYPE_UID_T

# Checks for library functions.

AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_FUNCS([\
	dup2 \
	gettimeofday \
	memset \
	mkdir \
	rmdir \
	select \
	strcasecmp \
	strdup \
	strerror \
	strtol \
	strtoul \
	strtoull \
	uname \
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
