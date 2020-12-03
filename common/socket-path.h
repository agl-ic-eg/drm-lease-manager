#ifndef SOCKET_PATH_H
#define SOCKET_PATH_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/un.h>

bool sockaddr_set_lease_server_path(struct sockaddr_un *dest,
				    const char *lease_name);

#endif
