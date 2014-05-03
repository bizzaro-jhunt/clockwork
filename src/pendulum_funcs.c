#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pendulum_funcs.h"

/***************************************************/

static pn_word cwa_fs_is_file(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISREG(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_dir(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISDIR(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_chardev(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISCHR(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_blockdev(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISBLK(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_fifo(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISFIFO(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_symlink(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISLNK(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_socket(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISSOCK(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_exists(pn_machine *m)
{
	struct stat st;
	return lstat((const char *)m->A, &st) == 0 ? 0 : 1;
}

static pn_word cwa_fs_mkdir(pn_machine *m)
{
	return mkdir((const char *)m->A, 0777) == 0 ? 0 : 1;
}

static pn_word cwa_fs_mkfile(pn_machine *m)
{
	int fd = open((const char *)m->A, O_WRONLY|O_CREAT, 0644);
	if (fd < 0) return 1;
	close(fd);
	return 0;
}

static pn_word cwa_fs_symlink(pn_machine *m)
{
	return symlink((const char *)m->A, (const char *)m->B) == 0 ? 0 : 1;
}

static pn_word cwa_fs_link(pn_machine *m)
{
	return link((const char *)m->A, (const char *)m->B) == 0 ? 0 : 1;
}

static pn_word cwa_fs_chown(pn_machine *m)
{
	return chown((const char *)m->A, m->B, m->C) == 0 ? 0 : 1;
}

static pn_word cwa_fs_chmod(pn_machine *m)
{
	return chmod((const char *)m->A, m->D) == 0 ? 0 : 1;
}

static pn_word cwa_fs_unlink(pn_machine *m)
{
	return unlink((const char *)m->A) == 0 ? 0 : 1;
}

static pn_word cwa_fs_rmdir(pn_machine *m)
{
	return rmdir((const char *)m->A) == 0 ? 0 : 1;
}

/***************************************************/

int pendulum_funcs(pn_machine *m)
{
	pn_func(m, "FS.EXISTS?",   cwa_fs_exists);
	pn_func(m, "FS.FILE?",     cwa_fs_is_file);
	pn_func(m, "FS.DIR?",      cwa_fs_is_dir);
	pn_func(m, "FS.CHARDEV?",  cwa_fs_is_chardev);
	pn_func(m, "FS.BLOCKDEV?", cwa_fs_is_blockdev);
	pn_func(m, "FS.FIFO?",     cwa_fs_is_fifo);
	pn_func(m, "FS.SYMLINK?",  cwa_fs_is_symlink);
	pn_func(m, "FS.SOCKET?",   cwa_fs_is_socket);

	pn_func(m, "FS.MKDIR",     cwa_fs_mkdir);
	pn_func(m, "FS.MKFILE",    cwa_fs_mkfile);
	pn_func(m, "FS.SYMLINK",   cwa_fs_symlink);
	pn_func(m, "FS.HARDLINK",  cwa_fs_link);

	pn_func(m, "FS.UNLINK",  cwa_fs_unlink);
	pn_func(m, "FS.RMDIR",   cwa_fs_rmdir);

	pn_func(m, "FS.CHOWN",   cwa_fs_chown);
	pn_func(m, "FS.CHMOD",   cwa_fs_chmod);

	return 0;
}
