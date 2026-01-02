/**
 * @file client_manager.h
 * @brief Manages the list of connected clients on the Server.
 */

#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include "infrastructure/server_types.h"

/**
 * @brief Initializes the internal client list (e.g., sets head to NULL).
 */
void init_clients(void);

/**
 * @brief Frees all client nodes and their SSL contexts.
 */
void free_clients(void);

/**
 * @brief Adds a new client connection to the list.
 * @param fd The socket file descriptor.
 * @param ssl The SSL handle (transferred ownership to manager).
 */
void add_client(int fd, SSL *ssl);

/**
 * @brief Removes a client by FD, cleaning up SSL and memory.
 */
void remove_client(int fd);

Client *get_client_by_uid(uint32_t uid);
Client *get_client_by_fd(int fd);

#endif