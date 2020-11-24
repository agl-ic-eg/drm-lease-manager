#include "lease-manager.h"
#include "lease-server.h"
#include "log.h"

#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char *progname)
{
	printf("Usage: %s [OPTIONS] [<DRM device>]\n\n"
	       "Options:\n"
	       "-h, --help \tPrint this help\n"
	       "-v, --verbose \tEnable verbose debug messages\n",
	       progname);
}

const char *opts = "vh";
const struct option options[] = {
    {"help", no_argument, NULL, 'h'},
    {"verbose", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0},
};

int main(int argc, char **argv)
{
	char *device = "/dev/dri/card0";

	bool debug_log = false;

	int c;
	while ((c = getopt_long(argc, argv, opts, options, NULL)) != -1) {
		int ret = EXIT_FAILURE;
		switch (c) {
		case 'v':
			debug_log = true;
			break;
		case 'h':
			ret = EXIT_SUCCESS;
			/* fall through */
		default:
			usage(argv[0]);
			return ret;
		}
	}

	if (optind < argc)
		device = argv[optind];

	dlm_log_enable_debug(debug_log);

	struct lm *lm = lm_create(device);
	if (!lm) {
		ERROR_LOG("DRM Lease initialization failed\n");
		return EXIT_FAILURE;
	}

	uint32_t *ids = NULL;
	int count_ids = lm_get_lease_ids(lm, &ids);
	assert(count_ids > 0);

	struct ls *ls = ls_create(ids, count_ids);
	if (!ls) {
		lm_destroy(lm);
		ERROR_LOG("Client socket initialization failed\n");
		return EXIT_FAILURE;
	}

	struct ls_req req;
	while (ls_get_request(ls, &req)) {
		switch (req.type) {
		case LS_REQ_GET_LEASE: {
			int index = req.lease_index;
			int fd = lm_lease_grant(lm, index);
			if (fd < 0) {
				// TODO: Add the lease name to the error log
				ERROR_LOG("Can't fulfill lease request.\n");
				ls_disconnect_client(ls, index);
				break;
			}

			if (!ls_send_fd(ls, index, fd)) {
				// TODO: Add the lease name to the error log
				ERROR_LOG("Client communication error.\n");
				ls_disconnect_client(ls, index);
				lm_lease_revoke(lm, index);
			}
			break;
		}
		case LS_REQ_RELEASE_LEASE:
			ls_disconnect_client(ls, req.lease_index);
			lm_lease_revoke(lm, req.lease_index);
			break;
		default:
			ERROR_LOG("Internal error: Invalid lease request\n");
			goto done;
		}
	}
done:
	ls_destroy(ls);
	lm_destroy(lm);
	return EXIT_FAILURE;
}
