/**
 * @file group_service.h
 * @brief Client-side service for group and conversation management.
 */

#ifndef GROUP_SERVICE_H
#define GROUP_SERVICE_H

#include <stdint.h>

/**
 * @brief Sends a request to create a new group.
 * @return 1 if packet sent, 0 if validation failed.
 */
int service_create_group(const char *name, const char *desc, uint32_t *uids, int count);

/**
 * @brief Helper to create a private conversation (1-on-1).
 */
void service_create_private(uint32_t target_uid, const char *target_name);

void service_req_conversations(void);
void service_delete_group(uint32_t conv_id);
void service_req_members(uint32_t conv_id);
void service_kick_member(uint32_t conv_id, uint32_t target_uid);

#endif