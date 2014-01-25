#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.68)

AC_INIT(clockwork, 0.4.6, bugs@niftylogic.com)

AC_CONFIG_SRCDIR([src])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE(0.3.2)

# Checks for programs.
AC_PROG_YACC
AC_PROG_CC
AC_PROG_LEX
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB(ssl,
	SSL_library_init,,
	AC_MSG_ERROR(Missing OpenSSL libraries))

AC_CHECK_LIB(crypto,
	CRYPTO_lock,,
	AC_MSG_ERROR(Missing OpenSSL libraries))

AC_CHECK_LIB(pthread,
	pthread_create,,
	AC_MSG_ERROR(Missing pthread library))

AC_CHECK_LIB(readline,
	read_history,,
	AC_MSG_ERROR(Missing GNU readline library))

AC_CHECK_LIB(sqlite3,
	sqlite3_open,,
	AC_MSG_ERROR(Missing SQLite3 library))

AC_CHECK_LIB(augeas,
	aug_init,,
	AC_MSG_ERROR(Missing Augeas library))

# Checks for header files.

AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([assert.h])
AC_CHECK_HEADERS([augeas.h])
AC_CHECK_HEADERS([ctype.h])
AC_CHECK_HEADERS([errno.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([fts.h])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([glob.h])
AC_CHECK_HEADERS([grp.h])
AC_CHECK_HEADERS([libgen.h])
AC_CHECK_HEADERS([linux/limits.h])
AC_CHECK_HEADERS([netdb.h])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([pwd.h])
AC_CHECK_HEADERS([readline/history.h])
AC_CHECK_HEADERS([readline/readline.h])
AC_CHECK_HEADERS([shadow.h])
AC_CHECK_HEADERS([signal.h])
AC_CHECK_HEADERS([sqlite3.h])
AC_CHECK_HEADERS([stdarg.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdio.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([sys/select.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([sys/stat.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([sys/utsname.h])
AC_CHECK_HEADERS([sys/wait.h])
AC_CHECK_HEADERS([time.h])
AC_CHECK_HEADERS([unistd.h])

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

AC_CHECK_FUNCS([dup2])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([mkdir])
AC_CHECK_FUNCS([rmdir])
AC_CHECK_FUNCS([select])
AC_CHECK_FUNCS([strcasecmp])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([strtoul])
AC_CHECK_FUNCS([strtoull])
AC_CHECK_FUNCS([uname])

AC_OUTPUT(Makefile)
