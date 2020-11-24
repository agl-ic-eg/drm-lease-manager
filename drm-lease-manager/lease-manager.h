#ifndef LEASE_MANAGER_H
#define LEASE_MANAGER_H
#include <stddef.h>
#include <stdint.h>

struct lm;

struct lm *lm_create(const char *path);
void lm_destroy(struct lm *lm);

int lm_get_lease_ids(struct lm *lm, uint32_t **ids);

int lm_lease_grant(struct lm *lm, int index);
void lm_lease_revoke(struct lm *lm, int index);
#endif
