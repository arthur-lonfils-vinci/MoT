/**
 * @file auth_service.h
 * @brief Client-side service for Authentication (Register/Login).
 *
 * Handles constructing packets and managing the blocking flow
 * required for initial authentication.
 */

#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include "system/protocol.h"

/**
 * @brief Sends a registration request and waits for a response.
 * @return 1 if registration succeeded, 0 otherwise.
 */
int service_register(const char *email, const char *username, const char *password);

/**
 * @brief Sends a login request and waits for a response.
 * * If successful, populates `app.my_info` with user data.
 * @return 1 if login succeeded, 0 otherwise.
 */
int service_login(const char *email, const char *password);

/**
 * @brief Clears the session file.
 */
void service_logout(void);

/**
 * @brief Sends an async request to update user profile.
 */
void service_update_user(const char *username, const char *password);

#endif