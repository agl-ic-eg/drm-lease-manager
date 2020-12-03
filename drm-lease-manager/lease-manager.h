#ifndef LEASE_MANAGER_H
#define LEASE_MANAGER_H
#include "drm-lease.h"

struct lm;

struct lm *lm_create(const char *path);
void lm_destroy(struct lm *lm);

int lm_get_lease_handles(struct lm *lm, struct lease_handle ***lease_handles);

int lm_lease_grant(struct lm *lm, struct lease_handle *lease_handle);
void lm_lease_revoke(struct lm *lm, struct lease_handle *lease_handle);
#endif
