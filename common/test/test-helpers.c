#include "test-helpers.h"
#include <check.h>

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

int get_dummy_fd(void)
{
	return dup(STDIN_FILENO);
}

void check_fd_equality(int fd1, int fd2)
{
	struct stat s1, s2;
	ck_assert_int_eq(fstat(fd1, &s1), 0);
	ck_assert_int_eq(fstat(fd2, &s2), 0);
	ck_assert_int_eq(s1.st_dev, s2.st_dev);
	ck_assert_int_eq(s1.st_ino, s2.st_ino);
}

void check_fd_is_open(int fd)
{
	struct stat st;
	ck_assert_int_eq(fstat(fd, &st), 0);
}

void check_fd_is_closed(int fd)
{
	struct stat st;
	ck_assert_int_ne(fstat(fd, &st), 0);
	ck_assert_int_eq(errno, EBADF);
}
