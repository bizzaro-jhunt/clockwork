#
# configure.ac - Clockwork Autoconf File
#

AC_PREREQ(2.68)
AC_INIT([Clockwork], [2.0.15], [bugs@niftylogic.com])

################################################

AC_CONFIG_SRCDIR([src/cw.h])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])
AC_CONFIG_HEADERS(config.h)

################################################

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

################################################

AX_TEST_COVERAGE

################################################

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LEX
AC_PROG_LN_S
AC_PROG_YACC

################################################

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

################################################

PKG_CHECK_MODULES([AUGEAS], [augeas])

################################################

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
	libintl.h \
	limits.h \
	linux/limits.h \
	malloc.h \
	netdb.h \
	pthread.h \
	pwd.h \
	readline/history.h \
	readline/readline.h \
	shadow.h \
	signal.h \
	stdarg.h \
	stddef.h \
	stdint.h \
	stdio.h \
	stdlib.h \
	string.h \
	syslog.h \
	sys/select.h \
	sys/socket.h \
	sys/stat.h \
	sys/time.h \
	sys/types.h \
	sys/utsname.h \
	sys/wait.h \
	time.h \
	unistd.h \
	utime.h \
])

################################################

AC_C_INLINE

################################################

AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

################################################

AC_FUNC_ALLOCA
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC

AC_CHECK_FUNCS([\
	alarm \
	dup2 \
	fchdir \
	gethostname \
	gettimeofday \
	localtime_r \
	memset \
	mkdir \
	munmap \
	rmdir \
	select \
	setenv \
	strcasecmp \
	strchr \
	strdup \
	strerror \
	strrchr \
	strtol \
	strtoul \
	strtoull \
	uname \
	utime \
])

################################################

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
