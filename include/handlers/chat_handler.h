/**
 * @file chat_handler.h
 * @brief Server-side handlers for messaging.
 */

#ifndef CHAT_HANDLER_H
#define CHAT_HANDLER_H

#include "infrastructure/server_types.h"
#include "system/protocol.h"

/**
 * @brief Handles incoming text messages.
 * Logs message to DB and routes it to online participants.
 */
void handle_send_text(Client *cli, const SendMessagePayload *payload);

/**
 * @brief Fetches and sends conversation history.
 */
void handle_req_history(Client *cli, const RequestHistoryPayload *payload);

#endif