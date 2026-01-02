#include "handlers/group_handler.h"
#include "infrastructure/client_manager.h"
#include "system/storage.h"
#include "system/logger.h"
#include <string.h>

// Helper logic specific to groups
static void notify_group_update(uint32_t conv_id, uint32_t exclude_uid)
{
	uint32_t members[MAX_PARTICIPANTS];
	int count = storage_get_conv_participants(conv_id, members, MAX_PARTICIPANTS);

	for (int i = 0; i < count; i++)
	{
		if (members[i] == exclude_uid)
			continue;

		Client *c = get_client_by_uid(members[i]);
		if (c && c->is_online)
		{
			ConversationSummary summaries[50];
			int n = storage_get_user_conversations(members[i], summaries, 50);
			send_packet(c->ssl, MSG_RESP_CONVERSATIONS, summaries, n * sizeof(ConversationSummary));
		}
	}
}

void handle_create_conv(Client *cli, const CreateConvPayload *p)
{
	uint32_t uids[MAX_PARTICIPANTS];
	int count = p->participants_count;
	if (count > MAX_PARTICIPANTS)
		count = MAX_PARTICIPANTS;
	memcpy(uids, p->participant_uids, count * sizeof(uint32_t));

	uint32_t new_id = 0;
	int created = 0;

	// Check private uniqueness
	if (p->type == CONV_TYPE_PRIVATE && count == 2)
	{
		new_id = storage_find_private_conversation(uids[0], uids[1]);
	}

	if (new_id == 0)
	{
		new_id = storage_create_conversation(p->type, p->name, p->description, uids, count);
		created = 1;
	}

	send_packet(cli->ssl, MSG_CONV_CREATED, &new_id, sizeof(new_id));

	if (created && new_id > 0)
	{
		// Notify others
		for (int i = 0; i < count; i++)
		{
			uint32_t uid = uids[i];
			if (uid == cli->uid)
				continue;

			Client *target = get_client_by_uid(uid);
			if (target && target->is_online)
			{
				ConversationSummary summaries[50];
				int c = storage_get_user_conversations(uid, summaries, 50);
				send_packet(target->ssl, MSG_RESP_CONVERSATIONS, summaries, c * sizeof(ConversationSummary));
			}
		}
	}
}

void handle_req_conversations(Client *cli)
{
	ConversationSummary summaries[50];
	int count = storage_get_user_conversations(cli->uid, summaries, 50);
	send_packet(cli->ssl, MSG_RESP_CONVERSATIONS, summaries, count * sizeof(ConversationSummary));
}

void handle_update_group(Client *cli, const UpdateGroupPayload *p)
{
	if (storage_is_admin(p->conv_id, cli->uid))
	{
		storage_update_group(p->conv_id, p->new_name, p->new_desc);
		notify_group_update(p->conv_id, 0);
	}
}

void handle_add_member(Client *cli, const AddMemberPayload *p)
{
	if (storage_is_admin(p->conv_id, cli->uid))
	{
		uint32_t target_uid;
		if (storage_get_uid_by_code(p->target_friend_code, &target_uid))
		{
			if (storage_add_participant(p->conv_id, target_uid, 0))
			{
				send_packet(cli->ssl, MSG_MEMBER_ADDED, NULL, 0);
				notify_group_update(p->conv_id, 0);
			}
		}
	}
}

void handle_req_members(Client *cli, const ReqMembersPayload *p)
{
	GroupMemberSummary mems[MAX_MEMBERS];
	int count = storage_get_group_members(p->conv_id, mems, MAX_MEMBERS);
	send_packet(cli->ssl, MSG_RESP_MEMBERS, mems, count * sizeof(GroupMemberSummary));
}

void handle_kick_member(Client *cli, const KickMemberPayload *p)
{
	if (storage_is_admin(p->conv_id, cli->uid) && p->target_uid != cli->uid)
	{
		storage_remove_participant(p->conv_id, p->target_uid);
		notify_group_update(p->conv_id, 0);

		// Explicitly refresh the kicked user so group disappears
		Client *kicked = get_client_by_uid(p->target_uid);
		if (kicked && kicked->is_online)
		{
			ConversationSummary summaries[50];
			int n = storage_get_user_conversations(p->target_uid, summaries, 50);
			send_packet(kicked->ssl, MSG_RESP_CONVERSATIONS, summaries, n * sizeof(ConversationSummary));
		}
	}
}

void handle_delete_group(Client *cli, const DeleteGroupPayload *p)
{
	if (storage_is_admin(p->conv_id, cli->uid))
	{
		uint32_t members[MAX_PARTICIPANTS];
		int count = storage_get_conv_participants(p->conv_id, members, MAX_PARTICIPANTS);

		storage_delete_conversation(p->conv_id);

		// Notify former members
		for (int i = 0; i < count; i++)
		{
			Client *c = get_client_by_uid(members[i]);
			if (c && c->is_online)
			{
				ConversationSummary summaries[50];
				int n = storage_get_user_conversations(members[i], summaries, 50);
				send_packet(c->ssl, MSG_RESP_CONVERSATIONS, summaries, n * sizeof(ConversationSummary));
			}
		}
	}
}