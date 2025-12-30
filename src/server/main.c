#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include "protocol.h"
#include "storage.h"
#include "logger.h"

#define MAX_EVENTS 20
#define MAX_CLIENTS 100

typedef struct
{
	int fd;
	uint32_t uid;
	char username[MAX_NAME_LEN];
	int is_online;
} Client;

Client clients[MAX_CLIENTS];

void init_clients()
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		clients[i].fd = 0;
		clients[i].uid = 0;
	}
}

void add_client(int fd)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].fd == 0)
		{
			clients[i].fd = fd;
			clients[i].uid = 0;
			clients[i].is_online = 0;
			strcpy(clients[i].username, "");
			return;
		}
	}
}

void remove_client(int fd)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].fd == fd)
		{
			clients[i].fd = 0;
			clients[i].uid = 0;
			clients[i].is_online = 0;
			break;
		}
	}
}

Client *get_client_by_uid(uint32_t uid)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].fd != 0 && clients[i].uid == uid)
			return &clients[i];
	}
	return NULL;
}

// Helper to refresh conversation list for all members of a group
void notify_group_update(uint32_t conv_id, uint32_t exclude_uid)
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
			send_packet(c->fd, MSG_RESP_CONVERSATIONS, summaries, n * sizeof(ConversationSummary));
		}
	}
}

Client *get_client_by_fd(int fd)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].fd == fd)
			return &clients[i];
	}
	return NULL;
}

int main()
{
	signal(SIGPIPE, SIG_IGN);
	log_init("server");
	log_print(LOG_INFO, "Starting Server V4 (SQLite+UID)...");

	int server_fd, epoll_fd;
	struct sockaddr_in address;
	struct epoll_event ev, events[MAX_EVENTS];

	init_clients();

	storage_backup("data/messagerie.db");

	if (!storage_init("data/messagerie.db"))
	{
		log_print(LOG_ERROR, "Failed to init DB");
		return 1;
	}

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	bind(server_fd, (struct sockaddr *)&address, sizeof(address));
	listen(server_fd, 10);

	epoll_fd = epoll_create1(0);
	ev.events = EPOLLIN;
	ev.data.fd = server_fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

	printf("Server V4 (SQLite+UID) Running on %d...\n", PORT);
	log_print(LOG_INFO, "Server listening on port %d", PORT);

	while (1)
	{
		int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

		for (int i = 0; i < nfds; i++)
		{
			if (events[i].data.fd == server_fd)
			{
				struct sockaddr_in cli_addr;
				socklen_t len = sizeof(cli_addr);
				int new_sock = accept(server_fd, (struct sockaddr *)&cli_addr, &len);
				if (new_sock >= 0)
				{
					// Set Timeout
					struct timeval tv;
					tv.tv_sec = 2; // 2 Seconds timeout
					tv.tv_usec = 0;
					setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

					log_print(LOG_INFO, "New connection accepted (FD: %d)", new_sock);

					ev.events = EPOLLIN;
					ev.data.fd = new_sock;
					epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_sock, &ev);
					add_client(new_sock);
					log_print(LOG_INFO, "New connection accepted (FD: %d)", new_sock);
				}
			}
			else
			{
				int fd = events[i].data.fd;
				MessageType type;
				void *payload = NULL;
				uint32_t len;

				log_print(LOG_DEBUG, "Processing FD %d: waiting for packet...", fd);

				if (recv_packet(fd, &type, &payload, &len) <= 0)
				{
					Client *c = get_client_by_fd(fd);
					if (c && c->uid > 0)
					{
						log_print(LOG_INFO, "User %s (UID: %d) disconnected", c->username, c->uid);
					}
					else
					{
						log_print(LOG_DEBUG, "Client (FD: %d) disconnected before login", fd);
					}
					close(fd);
					remove_client(fd);
					continue;
				}

				Client *cli = get_client_by_fd(fd);
				log_print(LOG_DEBUG, "Packet received from FD %d", fd);
				if (!cli)
				{
					if (payload)
						free(payload);
					continue;
				}

				switch (type)
				{
				case MSG_REGISTER:
				{
					RegisterPayload *p = (RegisterPayload *)payload;
					char code[FRIEND_CODE_LEN];
					if (storage_register_user(p->email, p->username, p->password, code))
						send_packet(fd, MSG_REGISTER_SUCCESS, NULL, 0);
					else
						send_packet(fd, MSG_REGISTER_FAIL, NULL, 0);
					break;
				}
				case MSG_LOGIN:
				{
					LoginPayload *p = (LoginPayload *)payload;
					User u;
					log_print(LOG_INFO, "Login attempt from FD %d", fd);
					if (storage_check_credentials(p->email, p->password, &u))
					{
						strncpy(cli->username, u.username, MAX_NAME_LEN);
						cli->uid = u.uid;
						cli->is_online = 1;

						MyInfoPayload info;
						info.uid = u.uid;
						strncpy(info.username, u.username, MAX_NAME_LEN);
						strncpy(info.email, u.email, MAX_EMAIL_LEN);
						strncpy(info.friend_code, u.friend_code, FRIEND_CODE_LEN);
						send_packet(fd, MSG_LOGIN_SUCCESS, &info, sizeof(info));
						log_print(LOG_INFO, "Login attempt from FD %d", fd);
					}
					else
					{
						log_print(LOG_WARN, "Login failed from FD %d", fd);
						send_packet(fd, MSG_LOGIN_FAIL, NULL, 0);
					}
					break;
				}

				// --- CONVERSATIONS ---
				case MSG_CREATE_CONV:
				{
					CreateConvPayload *p = (CreateConvPayload *)payload;

					// Copy packed uids to aligned array
					uint32_t uids[MAX_PARTICIPANTS];
					int count = p->participants_count;
					if (count > MAX_PARTICIPANTS)
						count = MAX_PARTICIPANTS;
					memcpy(uids, p->participant_uids, count * sizeof(uint32_t));

					uint32_t new_id = 0;
					int created = 0; // Flag to check if we created a new one

					// Uniqueness Check for Private Chats
					if (p->type == CONV_TYPE_PRIVATE && count == 2)
					{
						new_id = storage_find_private_conversation(uids[0], uids[1]);
					}

					if (new_id == 0)
					{
						new_id = storage_create_conversation(p->type, p->name, p->description, uids, count);
						created = 1;
					}

					// Notify the creator
					send_packet(fd, MSG_CONV_CREATED, &new_id, sizeof(new_id));

					// NOTIFICATION LOGIC:
					// If a new conversation was created, notify ALL other participants
					if (created && new_id > 0)
					{
						for (int i = 0; i < count; i++)
						{
							uint32_t uid = uids[i];
							if (uid == cli->uid)
								continue; // Don't notify self (already handled by MSG_CONV_CREATED)

							Client *target = get_client_by_uid(uid);
							if (target && target->is_online)
							{
								// Force refresh their conversation list
								ConversationSummary summaries[50];
								int c = storage_get_user_conversations(uid, summaries, 50);
								send_packet(target->fd, MSG_RESP_CONVERSATIONS, summaries, c * sizeof(ConversationSummary));
							}
						}
					}
					break;
				}
				case MSG_REQ_CONVERSATIONS:
				{
					ConversationSummary summaries[50];
					int count = storage_get_user_conversations(cli->uid, summaries, 50);
					send_packet(fd, MSG_RESP_CONVERSATIONS, summaries, count * sizeof(ConversationSummary));
					break;
				}
				case MSG_SEND_TEXT:
				{
					if (cli)
						log_print(LOG_INFO, "User %s sent message", cli->username);
					SendMessagePayload *p = (SendMessagePayload *)payload;
					storage_log_message(p->conv_id, cli->uid, p->text);

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
							send_packet(target_cli->fd, MSG_RTE_TEXT, &rp, sizeof(rp));
						}
					}
					break;
				}

				// --- HISTORY ---
				case MSG_REQ_HISTORY:
				{
					RequestHistoryPayload *p = (RequestHistoryPayload *)payload;
					char *hist = storage_get_history(p->conv_id);
					send_packet(fd, MSG_RESP_HISTORY, hist, strlen(hist));
					free(hist);
					break;
				}

				// --- CONTACTS/REQUESTS ---
				case MSG_REQ_CONTACTS:
				{
					ContactSummary contacts[50];
					int count = storage_get_contacts_data(cli->uid, contacts, 50);
					send_packet(fd, MSG_RESP_CONTACTS, contacts, count * sizeof(ContactSummary));
					break;
				}
				case MSG_ADD_BY_CODE:
				{
					AddContactPayload *p = (AddContactPayload *)payload;
					uint32_t target_uid;
					if (storage_get_uid_by_code(p->friend_code, &target_uid))
					{
						if (storage_add_request(cli->uid, target_uid))
						{
							send_packet(fd, MSG_ADD_REQ_SENT, NULL, 0);
							Client *tc = get_client_by_uid(target_uid);
							if (tc && tc->is_online)
							{
								ContactSummary reqs[50];
								int count = storage_get_requests_data(target_uid, reqs, 50);
								send_packet(tc->fd, MSG_RESP_REQUESTS, reqs, count * sizeof(ContactSummary));
							}
						}
						else
							send_packet(fd, MSG_ADD_FAIL, NULL, 0);
					}
					else
						send_packet(fd, MSG_ADD_FAIL, NULL, 0);
					break;
				}
				case MSG_GET_REQUESTS:
				{
					ContactSummary reqs[50];
					int count = storage_get_requests_data(cli->uid, reqs, 50);
					send_packet(fd, MSG_RESP_REQUESTS, reqs, count * sizeof(ContactSummary));
					break;
				}
				case MSG_DECIDE_REQUEST:
				{
					DecideRequestPayload *p = (DecideRequestPayload *)payload;
					if (p->accepted)
					{
						storage_add_friendship(cli->uid, p->target_uid);
						// Auto-create Private Chat
						uint32_t uids[2] = {cli->uid, p->target_uid};
						uint32_t conv_id = storage_find_private_conversation(cli->uid, p->target_uid);

						int new_conv = 0;
						if (conv_id == 0)
						{
							conv_id = storage_create_conversation(CONV_TYPE_PRIVATE, "Private Chat", "", uids, 2);
							new_conv = 1;
						}

						if (new_conv)
						{
							// 1. Refresh Me (The Accepter)
							ConversationSummary my_convs[50];
							int c1 = storage_get_user_conversations(cli->uid, my_convs, 50);
							send_packet(fd, MSG_RESP_CONVERSATIONS, my_convs, c1 * sizeof(ConversationSummary));

							// 2. Refresh Them (The Requester)
							Client *orig = get_client_by_uid(p->target_uid);
							if (orig && orig->is_online)
							{
								ConversationSummary their_convs[50];
								int c2 = storage_get_user_conversations(p->target_uid, their_convs, 50);
								send_packet(orig->fd, MSG_RESP_CONVERSATIONS, their_convs, c2 * sizeof(ConversationSummary));
							}
						}
					}
					storage_remove_request(p->target_uid, cli->uid);

					// Refresh Me
					ContactSummary my_contacts[50];
					int c_count = storage_get_contacts_data(cli->uid, my_contacts, 50);
					send_packet(fd, MSG_RESP_CONTACTS, my_contacts, c_count * sizeof(ContactSummary));

					ContactSummary my_reqs[50];
					int r_count = storage_get_requests_data(cli->uid, my_reqs, 50);
					send_packet(fd, MSG_RESP_REQUESTS, my_reqs, r_count * sizeof(ContactSummary));

					// Refresh Sender (if accepted)
					if (p->accepted)
					{
						Client *orig = get_client_by_uid(p->target_uid);
						if (orig && orig->is_online)
						{
							ContactSummary sender_contacts[50];
							int s_count = storage_get_contacts_data(p->target_uid, sender_contacts, 50);
							send_packet(orig->fd, MSG_RESP_CONTACTS, sender_contacts, s_count * sizeof(ContactSummary));
						}
					}
					break;
				}
				case MSG_UPDATE_USER:
				{
					UpdateUserPayload *p = (UpdateUserPayload *)payload;
					storage_update_user(cli->uid, p->new_username, p->new_password);
					if (strlen(p->new_username) > 0)
						strncpy(cli->username, p->new_username, MAX_NAME_LEN);
					send_packet(fd, MSG_UPDATE_SUCCESS, NULL, 0);
					break;
				}
				case MSG_UPDATE_GROUP:
				{
					UpdateGroupPayload *p = (UpdateGroupPayload *)payload;
					if (storage_is_admin(p->conv_id, cli->uid))
					{
						storage_update_group(p->conv_id, p->new_name, p->new_desc);
						// NOTIFY EVERYONE (0 as exclude means notify all)
						notify_group_update(p->conv_id, 0);
					}
					break;
				}
				case MSG_ADD_MEMBER:
				{
					AddMemberPayload *p = (AddMemberPayload *)payload;
					if (storage_is_admin(p->conv_id, cli->uid))
					{
						uint32_t target_uid;
						if (storage_get_uid_by_code(p->target_friend_code, &target_uid))
						{
							if (storage_add_participant(p->conv_id, target_uid, 0))
							{
								send_packet(fd, MSG_MEMBER_ADDED, NULL, 0);
								// Notify everyone (including the new guy)
								notify_group_update(p->conv_id, 0);
							}
						}
					}
					break;
				}
				case MSG_REQ_MEMBERS:
				{
					ReqMembersPayload *p = (ReqMembersPayload *)payload;
					// Ideally check if cli->uid is in the group first
					GroupMemberSummary mems[MAX_MEMBERS];
					int count = storage_get_group_members(p->conv_id, mems, MAX_MEMBERS);
					send_packet(fd, MSG_RESP_MEMBERS, mems, count * sizeof(GroupMemberSummary));
					break;
				}
				case MSG_KICK_MEMBER:
				{
					KickMemberPayload *p = (KickMemberPayload *)payload;
					if (storage_is_admin(p->conv_id, cli->uid) && p->target_uid != cli->uid)
					{
						storage_remove_participant(p->conv_id, p->target_uid);

						// Notify remaining members
						notify_group_update(p->conv_id, 0);

						// Notify the kicked user explicitly (so the group vanishes from their list)
						Client *kicked = get_client_by_uid(p->target_uid);
						if (kicked && kicked->is_online)
						{
							ConversationSummary summaries[50];
							int n = storage_get_user_conversations(p->target_uid, summaries, 50);
							send_packet(kicked->fd, MSG_RESP_CONVERSATIONS, summaries, n * sizeof(ConversationSummary));
						}
					}
					break;
				}
				case MSG_DELETE_GROUP:
				{
					DeleteGroupPayload *p = (DeleteGroupPayload *)payload;
					if (storage_is_admin(p->conv_id, cli->uid))
					{
						// 1. Get list of all members BEFORE deleting (to notify them)
						uint32_t members[MAX_PARTICIPANTS];
						int count = storage_get_conv_participants(p->conv_id, members, MAX_PARTICIPANTS);

						// 2. Delete
						storage_delete_conversation(p->conv_id);

						// 3. Notify all former members
						for (int i = 0; i < count; i++)
						{
							Client *c = get_client_by_uid(members[i]);
							if (c && c->is_online)
							{
								ConversationSummary summaries[50];
								int n = storage_get_user_conversations(members[i], summaries, 50);
								send_packet(c->fd, MSG_RESP_CONVERSATIONS, summaries, n * sizeof(ConversationSummary));
							}
						}
					}
					break;
				}
				default:
					break;
				}
				if (payload)
					free(payload);
			}
		}
	}
	storage_close();
	log_print(LOG_INFO, "Server shutting down");
	log_close();
	return 0;
}