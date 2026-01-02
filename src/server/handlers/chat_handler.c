#include "handlers/chat_handler.h"
#include "infrastructure/client_manager.h"
#include "system/storage.h"
#include "system/logger.h"
#include <stdlib.h>
#include <string.h>

void handle_send_text(Client *cli, const SendMessagePayload *p)
{
	log_print(LOG_INFO, "User %s sent message to conv %d", cli->username, p->conv_id);

	// 1. Log to DB
	storage_log_message(p->conv_id, cli->uid, p->text);

	// 2. Notify Participants
	uint32_t members[MAX_PARTICIPANTS];
	int count = storage_get_conv_participants(p->conv_id, members, MAX_PARTICIPANTS);

	for (int i = 0; i < count; i++)
	{
		uint32_t target_uid = members[i];
		if (target_uid == cli->uid)
			continue;

		Client *target_cli = get_client_by_uid(target_uid);
		if (target_cli && target_cli->is_online)
		{
			RoutedMessagePayload rp;
			rp.conv_id = p->conv_id;
			rp.sender_uid = cli->uid;
			strncpy(rp.sender_username, cli->username, MAX_NAME_LEN);
			strncpy(rp.text, p->text, MAX_TEXT_LEN);
			send_packet(target_cli->ssl, MSG_RTE_TEXT, &rp, sizeof(rp));
		}
	}
}

void handle_req_history(Client *cli, const RequestHistoryPayload *p)
{
	char *hist = storage_get_history(p->conv_id);
	if (hist)
	{
		send_packet(cli->ssl, MSG_RESP_HISTORY, hist, strlen(hist));
		free(hist);
	}
	else
	{
		// Send empty history if null
		send_packet(cli->ssl, MSG_RESP_HISTORY, "", 0);
	}
}