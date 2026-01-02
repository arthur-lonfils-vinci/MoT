/**
 * @file auth_handler.h
 * @brief Server-side handlers for authentication messages.
 */

#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

#include "infrastructure/server_types.h"
#include "system/protocol.h"

void handle_register(Client *cli, const RegisterPayload *payload);
void handle_login(Client *cli, const LoginPayload *payload);
void handle_update_user(Client *cli, const UpdateUserPayload *payload);

#endif