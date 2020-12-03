#include "socket-path.h"
#include "config.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOCKET_PATH STATEDIR "/run/drm-lease-manager"

bool sockaddr_set_lease_server_path(struct sockaddr_un *sa,
				    const char *lease_name)
{
	int maxlen = sizeof(sa->sun_path);
	char *socket_dir = getenv("DLM_SOCKET_PATH") ?: SOCKET_PATH;

	int len =
	    snprintf(sa->sun_path, maxlen, "%s/%s", socket_dir, lease_name);

	if (len < 0) {
		DEBUG_LOG("Socket path creation failed: %s\n", strerror(errno));
		return false;
	}
	if (len >= maxlen) {
		errno = ENAMETOOLONG;
		DEBUG_LOG("Socket directoy path too long. "
			  "Full path to socket must be less than %d bytes\n",
			  maxlen);
		return false;
	}
	return true;
}
