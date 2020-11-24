#include "lease-server.h"
#include "log.h"
#include "socket-path.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

struct ls_socket {
	int fd;
	struct ls_server *serv;
};

struct ls_server {
	int index;
	struct sockaddr_un address;

	struct ls_socket listen;
	struct ls_socket client;

	bool is_client_connected;
};

struct ls {
	int epoll_fd;

	struct ls_server *servers;
	int nservers;
};

static bool client_connect(struct ls *ls, struct ls_server *serv)
{
	int cfd = accept(serv->listen.fd, NULL, NULL);
	if (cfd < 0) {
		DEBUG_LOG("accept failed on %s: %s\n", serv->address.sun_path,
			  strerror(errno));
		return false;
	}

	if (serv->is_client_connected) {
		WARN_LOG("Client already connected on %s\n",
			 serv->address.sun_path);
		close(cfd);
		return false;
	}

	serv->client.fd = cfd;
	serv->client.serv = serv;

	struct epoll_event ev = {
	    .events = POLLHUP,
	    .data.ptr = &serv->client,
	};
	if (epoll_ctl(ls->epoll_fd, EPOLL_CTL_ADD, cfd, &ev)) {
		DEBUG_LOG("epoll_ctl add failed: %s\n", strerror(errno));
		close(cfd);
		return false;
	}

	serv->is_client_connected = true;
	return true;
}

static void client_disconnect(const struct ls *ls, struct ls_server *serv)
{
	if (!serv->is_client_connected)
		return;

	epoll_ctl(ls->epoll_fd, EPOLL_CTL_DEL, serv->client.fd, NULL);
	close(serv->client.fd);
	serv->is_client_connected = false;
}

static bool server_setup(const struct ls *ls, struct ls_server *serv,
			 uint32_t lease_id)
{
	struct sockaddr_un *address = &serv->address;

	if (!sockaddr_set_lease_server_path(address, lease_id))
		return false;

	address->sun_family = AF_UNIX;

	int server_socket = socket(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
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

	serv->is_client_connected = false;
	serv->listen.fd = server_socket;
	serv->listen.serv = serv;

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

	INFO_LOG("Lease server (id = %d) initialized at %s\n", lease_id,
		 address->sun_path);
	return true;
}

static void server_shutdown(const struct ls *ls, struct ls_server *serv)
{
	if (unlink(serv->address.sun_path)) {
		WARN_LOG("Server socket %s delete failed: %s\n",
			 serv->address.sun_path, strerror(errno));
	}

	epoll_ctl(ls->epoll_fd, EPOLL_CTL_DEL, serv->listen.fd, NULL);
	close(serv->listen.fd);
	client_disconnect(ls, serv);
}

struct ls *ls_create(uint32_t *ids, int count)
{
	assert(ids);
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
		ls->servers[i].index = i;
		if (!server_setup(ls, &ls->servers[i], ids[i]))
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

		struct ls_server *server = sock->serv;
		req->lease_index = server->index;

		if (sock == &server->listen) {
			if (!(ev.events & POLLIN))
				continue;
			if (client_connect(ls, server))
				request = LS_REQ_GET_LEASE;
		} else if (sock == &server->client) {
			if (!(ev.events & POLLHUP))
				continue;
			request = LS_REQ_RELEASE_LEASE;
		} else {
			ERROR_LOG("Internal error: Invalid socket context\n");
			return false;
		}
	}
	req->type = request;
	return true;
}

bool ls_send_fd(struct ls *ls, int lease_index, int fd)
{
	assert(ls);
	assert(lease_index >= 0 && lease_index < ls->nservers);

	if (fd < 0)
		return false;

	struct ls_server *serv = &ls->servers[lease_index];

	char data[1];
	struct iovec iov = {
	    .iov_base = data,
	    .iov_len = sizeof(data),
	};

	char ctrl_buf[CMSG_SPACE(sizeof(int))] = {0};

	struct msghdr msg = {
	    .msg_iov = &iov,
	    .msg_iovlen = 1,
	    .msg_controllen = sizeof(ctrl_buf),
	    .msg_control = ctrl_buf,
	};

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	*((int *)CMSG_DATA(cmsg)) = fd;

	if (sendmsg(serv->client.fd, &msg, 0) < 0) {
		DEBUG_LOG("sendmsg failed on %s: %s\n", serv->address.sun_path,
			  strerror(errno));
		return false;
	}

	INFO_LOG("Lease request granted on %s\n", serv->address.sun_path);
	return true;
}

void ls_disconnect_client(struct ls *ls, int lease_index)
{
	assert(ls);
	assert(lease_index >= 0 && lease_index < ls->nservers);

	struct ls_server *serv = &ls->servers[lease_index];
	client_disconnect(ls, serv);
}
