/**
 * @file contact_handler.h
 * @brief Server-side handlers for friend requests and contact lists.
 */

#ifndef CONTACT_HANDLER_H
#define CONTACT_HANDLER_H

#include "infrastructure/server_types.h"
#include "system/protocol.h"

void handle_req_contacts(Client *cli);
void handle_add_by_code(Client *cli, const AddContactPayload *payload);
void handle_get_requests(Client *cli);
void handle_decide_request(Client *cli, const DecideRequestPayload *payload);

#endif