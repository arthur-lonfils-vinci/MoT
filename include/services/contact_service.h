/**
 * @file contact_service.h
 * @brief Client-side service for friend management.
 */

#ifndef CONTACT_SERVICE_H
#define CONTACT_SERVICE_H

#include <stdint.h>

void service_refresh_contacts(void);
void service_refresh_requests(void);

/**
 * @brief Sends a friend request using a friend code.
 */
void service_add_contact(const char *friend_code);

/**
 * @brief Accepts or denies a friend request.
 * @param accept 1 to accept, 0 to deny.
 */
void service_decide_request(uint32_t target_uid, int accept);

#endif