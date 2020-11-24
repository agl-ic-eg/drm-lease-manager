#include "dlmclient.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xf86drmMode.h>

static void usage(const char *name)
{
	fprintf(stderr,
		"%s <connector_id>\n"
		"\tconnector_id: Valid value is CONNECTOR-ID provided by DRM\n",
		name);
}

static void dumpDRMResources(int lease_fd)
{
	drmModeObjectListPtr ol = drmModeGetLease(lease_fd);
	if (!ol) {
		fprintf(stderr, "drmModeGetLease failed\n");
		return;
	}

	drmModeRes *res = drmModeGetResources(lease_fd);
	if (!res) {
		fprintf(stderr, "drmModeGetResources failed\n");
		return;
	}

	for (int i = 0; i < res->count_crtcs; i++)
		printf("crtc-id: %u\n", res->crtcs[i]);

	for (int i = 0; i < res->count_connectors; i++)
		printf("connector-id: %u\n", res->connectors[i]);

	drmModeFreeResources(res);

	drmModePlaneRes *plane_res = drmModeGetPlaneResources(lease_fd);
	if (!plane_res) {
		fprintf(stderr, "drmModeGetPlaneResources failed\n");
		return;
	}

	for (uint32_t i = 0; i < plane_res->count_planes; i++)
		printf("plane-id: %u\n", plane_res->planes[i]);

	drmModeFreePlaneResources(plane_res);
}

int main(int argc, char **argv)
{
	const int id = (argc < 2) ? -1 : atoi(argv[1]);
	if (id < 0) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	struct dlm_lease *lease = dlm_get_lease((uint32_t)id);
	if (!lease) {
		fprintf(stderr, "dlm_get_lease: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	int lease_fd = dlm_lease_fd(lease);
	if (lease_fd < 0) {
		fprintf(stderr, "dlm_lease_fd: %s\n", strerror(errno));
		dlm_release_lease(lease);
		return EXIT_FAILURE;
	}

	dumpDRMResources(lease_fd);
	dlm_release_lease(lease);
	return EXIT_SUCCESS;
}
