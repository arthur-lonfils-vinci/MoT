/**
 * @file chat_service.h
 * @brief Client-side service for sending messages and history.
 */

#ifndef CHAT_SERVICE_H
#define CHAT_SERVICE_H

#include <stdint.h>

/**
 * @brief Sends a text message to the specified conversation.
 */
void service_send_text(uint32_t conv_id, const char *text);

/**
 * @brief Requests the message history for a conversation.
 */
void service_req_history(uint32_t conv_id);

#endif