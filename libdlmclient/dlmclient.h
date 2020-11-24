/**
 * @file dlmclient.h
 */
#ifndef DLM_CLIENT_H
#define DLM_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/*
 * @brief Enable debug logging
 * @param enable [in] true - enable debug logging
 *                   false - disable debug logging
 */
void dlm_enable_debug_log(bool enable);

/**
 * @brief lease handle
 */
struct dlm_lease;

/**
 * @brief  Get a DRM lease from the lease manager
 *
 * @param id [in] id of lease to request
 * @return A pointer to a lease handle on success.
 *         On error this function returns NULL and errno is set accordingly.
 *
 *  Possible errors:
 *
 *  EACCESS         Cannot access lease manager socket directory
 *  EACCESS         Lease request denied by lease manager
 *  ENAMETOOLONG    The path to the lease manager socket directory is too long
 *  ENOENT          Lease manager or requested lease not available
 *  ENOMEM          Out of memory during operation
 *  EPROTO          Protocol error in communication with lease manager
 *
 *  This list is not exhaustive, and errno may be set to other error codes,
 *  especially those related to socket communication.
 */
struct dlm_lease *dlm_get_lease(uint32_t id);

/**
 * @brief  Release a lease handle
 *
 * @details Release a lease handle.  The lease handle will be invalidated and
 *          the associated DRM lease wil be revoked.  Any fd's retrieved from
 *          dlm_lease_fd() will be closed.
 * @param handle [in] pointer to lease handle
 */
void dlm_release_lease(struct dlm_lease *lease);

/**
 * @brief Get a DRM Master fd from a valid lease handle
 *
 * @param lease [in] valid pointer to a lease handle
 * @return A DRM Master file descriptor for the lease on success.
 *         -1 is returned when called with a NULL lease handle.
 */
int dlm_lease_fd(struct dlm_lease *lease);

#ifdef __cplusplus
}
#endif

#endif
