#ifndef TEST_SOCKET_SERVER_H
#define TEST_SOCKET_SERVER_H
#include <stdbool.h>

struct test_config {
	char *lease_name;
	int nfds;
	int *fds;

	bool send_data_without_fd;
	bool send_no_data;
};

void test_config_cleanup(struct test_config *config);

struct server_state *test_server_start(struct test_config *test_config);
void test_server_stop(struct server_state *sstate);
#endif
