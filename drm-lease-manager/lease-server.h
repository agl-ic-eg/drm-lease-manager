#ifndef LEASE_SERVER_H
#define LEASE_SERVER_H
#include <stdbool.h>

#include "drm-lease.h"

struct ls;
struct ls_server;
enum ls_req_type {
	LS_REQ_GET_LEASE,
	LS_REQ_RELEASE_LEASE,
};

struct ls_req {
	struct lease_handle *lease_handle;
	struct ls_server *server;
	enum ls_req_type type;
};

struct ls *ls_create(struct lease_handle **lease_handles, int count);
void ls_destroy(struct ls *ls);

bool ls_get_request(struct ls *ls, struct ls_req *req);
bool ls_send_fd(struct ls *ls, struct ls_server *server, int fd);

void ls_disconnect_client(struct ls *ls, struct ls_server *server);
#endif
