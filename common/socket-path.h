#ifndef SOCKET_PATH_H
#define SOCKET_PATH_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/un.h>

bool sockaddr_set_lease_server_path(struct sockaddr_un *dest,
				    uint32_t lease_id);

#endif
