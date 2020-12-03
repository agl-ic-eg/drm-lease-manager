#ifndef TEST_SOCKET_CLIENT_H
#define TEST_SOCKET_CLIENT_H
#include <stdbool.h>

#include "drm-lease.h"
struct test_config {
	// settings
	struct lease_handle *lease;
	int recv_timeout;

	// outputs
	int received_fd;
	bool has_data;
	bool connection_completed;
};

void test_config_cleanup(struct test_config *config);

struct client_state;
struct client_state *test_client_start(struct test_config *test_config);
void test_client_stop(struct client_state *cstate);
#endif
