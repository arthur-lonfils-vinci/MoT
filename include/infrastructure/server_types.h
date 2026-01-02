/**
 * @file server_types.h
 * @brief shared structures for server-side state.
 */

#ifndef SERVER_TYPES_H
#define SERVER_TYPES_H

#include <stdint.h>
#include "system/protocol.h"

/**
 * @brief Represents a connected client on the server.
 */
typedef struct
{
    int fd;         /**< File descriptor for the socket */
    SSL *ssl;       /**< SSL connection handle */
    uint32_t uid;   /**< Authenticated User ID (0 if guest) */
    char username[MAX_NAME_LEN];
    int is_online;  /**< Status flag */
} Client;

#endif