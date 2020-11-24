#include "socket-path.h"
#include "config.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOCKET_PATH STATEDIR "/run/drm-lease-manager"
#define SOCKET_FORMAT "dlm_connector-%u"

bool sockaddr_set_lease_server_path(struct sockaddr_un *sa, uint32_t lease_id)
{
	int maxlen = sizeof(sa->sun_path);
	char *socket_dir = getenv("DLM_SOCKET_PATH") ?: SOCKET_PATH;

	int len = snprintf(sa->sun_path, maxlen, "%s/" SOCKET_FORMAT,
			   socket_dir, lease_id);

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
