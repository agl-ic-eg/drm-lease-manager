#ifndef LEASE_SERVER_H
#define LEASE_SERVER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ls;
enum ls_req_type {
	LS_REQ_GET_LEASE,
	LS_REQ_RELEASE_LEASE,
};

struct ls_req {
	int lease_index;
	enum ls_req_type type;
};

struct ls *ls_create(uint32_t *ids, int count);
void ls_destroy(struct ls *ls);

bool ls_get_request(struct ls *ls, struct ls_req *req);
bool ls_send_fd(struct ls *ls, int lease_index, int fd);

void ls_disconnect_client(struct ls *ls, int lease_index);
#endif
