#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#define UNUSED(x) (void)(x)
#define ARRAY_LEN(x) ((int)(sizeof(x) / sizeof(x[0])))

/* Get a vaild fd to use a a placeholder.
 * The dummy fd should never be used for anything other
 * than comparing the fd value or the referenced file description. */
int get_dummy_fd(void);

void check_fd_equality(int fd1, int fd2);
void check_fd_is_open(int fd);
void check_fd_is_closed(int fd);
#endif
