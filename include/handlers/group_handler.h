/**
 * @file group_handler.h
 * @brief Server-side handlers for group administration.
 */

#ifndef GROUP_HANDLER_H
#define GROUP_HANDLER_H

#include "infrastructure/server_types.h"
#include "system/protocol.h"

void handle_create_conv(Client *cli, const CreateConvPayload *payload);
void handle_req_conversations(Client *cli);
void handle_update_group(Client *cli, const UpdateGroupPayload *payload);
void handle_add_member(Client *cli, const AddMemberPayload *payload);
void handle_req_members(Client *cli, const ReqMembersPayload *payload);
void handle_kick_member(Client *cli, const KickMemberPayload *payload);
void handle_delete_group(Client *cli, const DeleteGroupPayload *payload);

#endif