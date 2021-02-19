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
