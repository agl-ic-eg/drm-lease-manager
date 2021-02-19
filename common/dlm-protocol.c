/* Copyright 2020-2021 IGEL Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dlm-protocol.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

bool receive_dlm_client_request(int socket, struct dlm_client_request *request)
{

	ssize_t len;
	struct iovec iov = {
	    .iov_base = request,
	    .iov_len = sizeof(*request),
	};
	struct msghdr msg = {
	    .msg_iov = &iov,
	    .msg_iovlen = 1,
	};

	while ((len = recvmsg(socket, &msg, 0)) < 0) {
		if (errno != EINTR)
			return false;
	}

	if (len != sizeof(*request)) {
		errno = EPROTO;
		return false;
	}
	return true;
}

bool send_dlm_client_request(int socket, struct dlm_client_request *request)
{
	struct iovec iov = {
	    .iov_base = request,
	    .iov_len = sizeof(*request),
	};

	struct msghdr msg = {
	    .msg_iov = &iov,
	    .msg_iovlen = 1,
	};

	while (sendmsg(socket, &msg, 0) < 1) {
		if (errno != EINTR)
			return false;
	}
	return true;
}

int receive_lease_fd(int socket)
{
	int lease_fd = -1;
	char ctrl_buf[CMSG_SPACE(sizeof(lease_fd))];

	char data;
	struct iovec iov = {.iov_base = &data, .iov_len = sizeof(data)};
	struct msghdr msg = {
	    .msg_iov = &iov,
	    .msg_iovlen = 1,
	    .msg_control = ctrl_buf,
	    .msg_controllen = sizeof(ctrl_buf),
	};

	ssize_t len;
	while ((len = recvmsg(socket, &msg, 0)) <= 0) {
		if (len == 0) {
			errno = EACCES;
			goto err;
		}

		if (errno != EINTR)
			goto err;
	}

	struct cmsghdr *cmsg;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS) {
			int nfds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
			int *fds = (int *)CMSG_DATA(cmsg);

			if (nfds == 1) {
				lease_fd = fds[0];
				break;
			}

			/* Close any unexpected fds so we don't leak them. */
			for (int i = 0; i < nfds; i++)
				close(fds[i]);
			break;
		}
	}

	if (lease_fd < 0) {
		errno = EPROTO;
		goto err;
	}

err:
	return lease_fd;
}

bool send_lease_fd(int socket, int lease)
{
	char data;
	struct iovec iov = {
	    .iov_base = &data,
	    .iov_len = sizeof(data),
	};

	char ctrl_buf[CMSG_SPACE(sizeof(lease))] = {0};

	struct msghdr msg = {
	    .msg_iov = &iov,
	    .msg_iovlen = 1,
	    .msg_controllen = sizeof(ctrl_buf),
	    .msg_control = ctrl_buf,
	};

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(lease));
	*((int *)CMSG_DATA(cmsg)) = lease;

	if (sendmsg(socket, &msg, 0) < 0)
		return false;

	return true;
}
