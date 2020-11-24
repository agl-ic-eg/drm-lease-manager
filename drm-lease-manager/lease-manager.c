#include "lease-manager.h"
#include "log.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/* Number of resources, excluding planes, to be included in each DRM lease.
 * Each lease needs at least a CRTC and conector. */
#define DRM_LEASE_MIN_RES (2)

struct lease {
	bool is_granted;
	uint32_t lessee_id;
	int lease_fd;

	uint32_t *object_ids;
	int nobject_ids;
};

struct lm {
	int drm_fd;
	drmModeResPtr drm_resource;
	drmModePlaneResPtr drm_plane_resource;

	uint32_t *lease_ids;
	struct lease **leases;
	int nleases;
};

static int drm_get_active_crtc_index(struct lm *lm,
				     drmModeConnectorPtr connector)
{
	drmModeEncoder *encoder =
	    drmModeGetEncoder(lm->drm_fd, connector->encoder_id);
	if (!encoder)
		return -1;

	uint32_t crtc_id = encoder->crtc_id;
	drmModeFreeEncoder(encoder);
	for (int i = 0; i < lm->drm_resource->count_crtcs; i++) {
		if (lm->drm_resource->crtcs[i] == crtc_id)
			return i;
	}
	return -1;
}

static int drm_get_crtc_index(struct lm *lm, uint32_t connector_id)
{
	drmModeConnectorPtr connector =
	    drmModeGetConnector(lm->drm_fd, connector_id);
	if (!connector)
		return -1;

	// try the active CRTC first
	int crtc_index = drm_get_active_crtc_index(lm, connector);
	if (crtc_index != -1)
		goto found;

	// If not try the first available CRTC on the connector/encoder
	for (int i = 0; i < connector->count_encoders; i++) {
		drmModeEncoder *encoder =
		    drmModeGetEncoder(lm->drm_fd, connector->encoders[i]);

		assert(encoder);

		int crtc = ffs(encoder->possible_crtcs);
		drmModeFreeEncoder(encoder);
		if (crtc == 0)
			continue;
		crtc_index = crtc - 1;
		break;
	}
found:
	drmModeFreeConnector(connector);
	return crtc_index;
}

static bool lease_add_planes(struct lm *lm, struct lease *lease, int crtc_index)
{
	for (uint32_t i = 0; i < lm->drm_plane_resource->count_planes; i++) {
		uint32_t plane_id = lm->drm_plane_resource->planes[i];
		drmModePlanePtr plane = drmModeGetPlane(lm->drm_fd, plane_id);

		assert(plane);

		// Exclude planes that can be used with multiple CRTCs for now
		if (plane->possible_crtcs == (1u << crtc_index)) {
			lease->object_ids[lease->nobject_ids++] = plane_id;
		}
		drmModeFreePlane(plane);
	}
	return true;
}

static void lease_free(struct lease *lease)
{
	free(lease->object_ids);
	free(lease);
}

static struct lease *lease_create(struct lm *lm, uint32_t connector_id)
{
	struct lease *lease = calloc(1, sizeof(struct lease));
	if (!lease) {
		DEBUG_LOG("Memory allocation failed: %s\n", strerror(errno));
		return NULL;
	}

	int nobjects = lm->drm_plane_resource->count_planes + DRM_LEASE_MIN_RES;
	lease->object_ids = calloc(nobjects, sizeof(uint32_t));
	if (!lease->object_ids) {
		DEBUG_LOG("Memory allocation failed: %s\n", strerror(errno));
		goto err;
	}

	int crtc_index = drm_get_crtc_index(lm, connector_id);
	if (crtc_index < 0) {
		DEBUG_LOG("No crtc found for connector %u\n", connector_id);
		goto err;
	}

	if (!lease_add_planes(lm, lease, crtc_index))
		goto err;

	uint32_t crtc_id = lm->drm_resource->crtcs[crtc_index];
	lease->object_ids[lease->nobject_ids++] = crtc_id;
	lease->object_ids[lease->nobject_ids++] = connector_id;

	lease->is_granted = false;

	return lease;

err:
	lease_free(lease);
	return NULL;
}

struct lm *lm_create(const char *device)
{
	struct lm *lm = calloc(1, sizeof(struct lm));
	if (!lm) {
		DEBUG_LOG("Memory allocation failed: %s\n", strerror(errno));
		return NULL;
	}
	lm->drm_fd = open(device, O_RDWR);
	if (lm->drm_fd < 0) {
		ERROR_LOG("Cannot open DRM device (%s): %s\n", device,
			  strerror(errno));
		goto err;
	}

	lm->drm_resource = drmModeGetResources(lm->drm_fd);
	if (!lm->drm_resource) {
		ERROR_LOG("Invalid DRM device(%s)\n", device);
		DEBUG_LOG("drmModeGetResources failed: %s\n", strerror(errno));
		goto err;
	}

	lm->drm_plane_resource = drmModeGetPlaneResources(lm->drm_fd);
	if (!lm->drm_plane_resource) {
		DEBUG_LOG("drmModeGetPlaneResources failed: %s\n",
			  strerror(errno));
		goto err;
	}

	lm->lease_ids = lm->drm_resource->connectors;
	int num_leases = lm->drm_resource->count_connectors;

	lm->leases = calloc(num_leases, sizeof(struct lease *));
	if (!lm->leases) {
		DEBUG_LOG("Memory allocation failed: %s\n", strerror(errno));
		goto err;
	}

	for (int i = 0; i < num_leases; i++) {
		uint32_t connector_id = lm->drm_resource->connectors[i];
		struct lease *lease = lease_create(lm, connector_id);
		if (!lease) {
			ERROR_LOG("Failed to initialize lease (id=%d)\n",
				  connector_id);
			goto err;
		}
		lm->leases[i] = lease;
		lm->nleases++;
	}
	return lm;

err:
	lm_destroy(lm);
	return NULL;
}

void lm_destroy(struct lm *lm)
{
	assert(lm);

	for (int i = 0; i < lm->nleases; i++) {
		lm_lease_revoke(lm, i);
		lease_free(lm->leases[i]);
	}

	free(lm->leases);
	drmModeFreeResources(lm->drm_resource);
	drmModeFreePlaneResources(lm->drm_plane_resource);
	close(lm->drm_fd);
	free(lm);
}

int lm_get_lease_ids(struct lm *lm, uint32_t **ids)
{
	assert(lm);
	assert(ids);

	*ids = lm->lease_ids;
	return lm->nleases;
}

int lm_lease_grant(struct lm *lm, int index)
{
	assert(lm);
	assert(index > -1 && index < lm->nleases);

	struct lease *lease = lm->leases[index];
	if (lease->is_granted)
		return lease->lease_fd;

	lease->lease_fd =
	    drmModeCreateLease(lm->drm_fd, lease->object_ids,
			       lease->nobject_ids, 0, &lease->lessee_id);
	if (lease->lease_fd < 0) {
		ERROR_LOG("drmModeCreateLease failed on lease %u: %s\n",
			  lm->lease_ids[index], strerror(errno));
		return -1;
	}

	lease->is_granted = true;
	return lease->lease_fd;
}

void lm_lease_revoke(struct lm *lm, int index)
{
	assert(lm);
	assert(index > -1 && index < lm->nleases);

	struct lease *lease = lm->leases[index];
	if (!lease->is_granted)
		return;

	drmModeRevokeLease(lm->drm_fd, lease->lessee_id);
	close(lease->lease_fd);
	lease->is_granted = false;
}
