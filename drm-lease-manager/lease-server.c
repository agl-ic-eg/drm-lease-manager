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

#include "lease-server.h"

#include "dlm-protocol.h"
#include "log.h"
#include "socket-path.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCK_LOCK_SUFFIX ".lock"

/* ACTIVE_CLIENTS
 * An 'active' client is one that either
 *  - owns a lease, or
 *  - is requesting ownership of a lease (which will
 *    disconnect the current owner if granted)
 *
 * There can only be at most one of each kind of client at the same
 * time. Any other client connections are queued in the
 * listen() backlog, waiting to be accept()'ed.
 */
#define ACTIVE_CLIENTS 2

struct ls_socket {
	int fd;
	bool is_server;
	union {
		struct ls_server *server;
		struct ls_client *client;
	};
};

struct ls_client {
	struct ls_socket socket;
	struct ls_server *serv;
	bool is_connected;
};

struct ls_server {
	struct lease_handle *lease_handle;
	struct sockaddr_un address;
	int server_socket_lock;

	struct ls_socket listen;
	struct ls_client clients[ACTIVE_CLIENTS];
};

struct ls {
	int epoll_fd;

	struct ls_server *servers;
	int nservers;
};

static void client_connect(struct ls *ls, struct ls_server *serv)
{
	int cfd = accept(serv->listen.fd, NULL, NULL);
	if (cfd < 0) {
		DEBUG_LOG("accept failed on %s: %s\n", serv->address.sun_path,
			  strerror(errno));
		return;
	}

	struct ls_client *client = NULL;

	for (int i = 0; i < ACTIVE_CLIENTS; i++) {
		if (!serv->clients[i].is_connected) {
			client = &serv->clients[i];
			break;
		}
	}
	if (!client) {
		close(cfd);
		return;
	}

	client->socket.fd = cfd;

	struct epoll_event ev = {
	    .events = POLLIN,
	    .data.ptr = &client->socket,
	};
	if (epoll_ctl(ls->epoll_fd, EPOLL_CTL_ADD, cfd, &ev)) {
		DEBUG_LOG("epoll_ctl add failed: %s\n", strerror(errno));
		close(cfd);
		return;
	}

	client->is_connected = true;
}

static int parse_client_request(struct ls_socket *client)
{
	int ret = -1;
	struct dlm_client_request hdr;
	if (!receive_dlm_client_request(client->fd, &hdr))
		return ret;

	switch (hdr.opcode) {
	case DLM_GET_LEASE:
		ret = LS_REQ_GET_LEASE;
		break;
	case DLM_RELEASE_LEASE:
		ret = LS_REQ_RELEASE_LEASE;
		break;
	default:
		ERROR_LOG("Unexpected client request received\n");
		break;
	};

	return ret;
}

static int create_socket_lock(struct sockaddr_un *addr)
{
	int lock_fd;

	int lockfile_len = sizeof(addr->sun_path) + sizeof(SOCK_LOCK_SUFFIX);
	char lockfile[lockfile_len];
	int len = snprintf(lockfile, lockfile_len, "%s%s", addr->sun_path,
			   SOCK_LOCK_SUFFIX);

	if (len < 0 || len >= lockfile_len) {
		DEBUG_LOG("Can't create socket lock filename\n");
		return -1;
	}

	lock_fd = open(lockfile, O_CREAT | O_RDWR,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

	if (lock_fd < 0) {
		ERROR_LOG("Cannot access runtime directory\n");
		return -1;
	}

	if (flock(lock_fd, LOCK_EX | LOCK_NB)) {
		ERROR_LOG(
		    "socket %s: in use.  Possible duplicate lease name or "
		    "mutiple drm-lease-manager instances running\n",
		    addr->sun_path);
		close(lock_fd);
		return -1;
	}

	return lock_fd;
}

static bool server_setup(struct ls *ls, struct ls_server *serv,
			 struct lease_handle *lease_handle)
{
	struct sockaddr_un *address = &serv->address;

	if (!sockaddr_set_lease_server_path(address, lease_handle->name))
		return false;

	int socket_lock = create_socket_lock(address);
	if (socket_lock < 0)
		return false;

	/* The socket address is now owned by this instance, so any existing
	 * sockets can safely be removed */
	unlink(address->sun_path);

	address->sun_family = AF_UNIX;

	int server_socket = socket(PF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);
	if (server_socket < 0) {
		DEBUG_LOG("Socket creation failed: %s\n", strerror(errno));
		return false;
	}

	if (bind(server_socket, (struct sockaddr *)address, sizeof(*address))) {
		ERROR_LOG("Failed to create named socket at %s: %s\n",
			  address->sun_path, strerror(errno));
		close(server_socket);
		return false;
	}

	if (listen(server_socket, 0)) {
		DEBUG_LOG("listen failed on %s: %s\n", address->sun_path,
			  strerror(errno));
		close(server_socket);
		unlink(address->sun_path);
		return false;
	}

	for (int i = 0; i < ACTIVE_CLIENTS; i++) {
		struct ls_client *client = &serv->clients[i];
		client->serv = serv;
		client->socket.client = client;
	}

	serv->lease_handle = lease_handle;
	serv->server_socket_lock = socket_lock;

	serv->listen.fd = server_socket;
	serv->listen.server = serv;
	serv->listen.is_server = true;

	struct epoll_event ev = {
	    .events = POLLIN,
	    .data.ptr = &serv->listen,
	};

	if (epoll_ctl(ls->epoll_fd, EPOLL_CTL_ADD, server_socket, &ev)) {
		DEBUG_LOG("epoll_ctl add failed: %s\n", strerror(errno));
		close(server_socket);
		unlink(address->sun_path);
		return false;
	}

	INFO_LOG("Lease server (%s) initialized at %s\n", lease_handle->name,
		 address->sun_path);
	return true;
}

static void server_shutdown(struct ls *ls, struct ls_server *serv)
{
	if (unlink(serv->address.sun_path)) {
		WARN_LOG("Server socket %s delete failed: %s\n",
			 serv->address.sun_path, strerror(errno));
	}

	epoll_ctl(ls->epoll_fd, EPOLL_CTL_DEL, serv->listen.fd, NULL);
	close(serv->listen.fd);

	for (int i = 0; i < ACTIVE_CLIENTS; i++)
		ls_disconnect_client(ls, &serv->clients[i]);

	close(serv->server_socket_lock);
}

struct ls *ls_create(struct lease_handle **lease_handles, int count)
{
	assert(lease_handles);
	assert(count > 0);

	struct ls *ls = calloc(1, sizeof(struct ls));
	if (!ls) {
		DEBUG_LOG("Memory allocation failed: %s\n", strerror(errno));
		return NULL;
	}

	ls->servers = calloc(count, sizeof(struct ls_server));
	if (!ls->servers) {
		DEBUG_LOG("Memory allocation failed: %s\n", strerror(errno));
		goto err;
	}

	ls->epoll_fd = epoll_create1(0);
	if (ls->epoll_fd < 0) {
		DEBUG_LOG("epoll_create failed: %s\n", strerror(errno));
		goto err;
	}

	for (int i = 0; i < count; i++) {
		if (!server_setup(ls, &ls->servers[i], lease_handles[i]))
			goto err;
		ls->nservers++;
	}
	return ls;
err:
	ls_destroy(ls);
	return NULL;
}

void ls_destroy(struct ls *ls)
{
	assert(ls);

	for (int i = 0; i < ls->nservers; i++)
		server_shutdown(ls, &ls->servers[i]);

	close(ls->epoll_fd);
	free(ls->servers);
	free(ls);
}

bool ls_get_request(struct ls *ls, struct ls_req *req)
{
	assert(ls);
	assert(req);

	int request = -1;
	while (request < 0) {
		struct epoll_event ev;
		if (epoll_wait(ls->epoll_fd, &ev, 1, -1) < 0) {
			if (errno == EINTR)
				continue;
			DEBUG_LOG("epoll_wait failed: %s\n", strerror(errno));
			return false;
		}

		struct ls_socket *sock = ev.data.ptr;
		assert(sock);

		if (sock->is_server) {
			if (ev.events & POLLIN)
				client_connect(ls, sock->server);
			continue;
		}

		if (ev.events & POLLIN)
			request = parse_client_request(sock);

		if (request < 0 && (ev.events & POLLHUP))
			request = LS_REQ_CLIENT_DISCONNECT;

		struct ls_client *client = sock->client;
		struct ls_server *server = client->serv;

		req->lease_handle = server->lease_handle;
		req->client = client;
		req->type = request;
	}
	return true;
}

bool ls_send_fd(struct ls *ls, struct ls_client *client, int fd)
{
	assert(ls);
	assert(client);

	struct ls_server *serv = client->serv;

	if (fd < 0)
		return false;

	if (!send_lease_fd(client->socket.fd, fd)) {
		DEBUG_LOG("sendmsg failed on %s: %s\n", serv->address.sun_path,
			  strerror(errno));
		return false;
	}

	if (fd > 0)
		INFO_LOG("Lease request granted on %s\n",
			 serv->address.sun_path);

	return true;
}

void ls_disconnect_client(struct ls *ls, struct ls_client *client)
{
	assert(ls);
	assert(client);

	if (!client->is_connected)
		return;

	epoll_ctl(ls->epoll_fd, EPOLL_CTL_DEL, client->socket.fd, NULL);
	close(client->socket.fd);
	client->is_connected = false;
}
