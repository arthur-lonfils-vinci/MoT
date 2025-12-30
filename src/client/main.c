#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "protocol.h"
#include "ui.h"
#include "logger.h"
#include "client_context.h"

void *network_thread(void *arg)
{
	(void)arg;
	MessageType type;
	void *payload;
	uint32_t len;

	while (recv_packet(app.sock_fd, &type, &payload, &len) > 0)
	{
		switch (type)
		{
		case MSG_RESP_CONVERSATIONS:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				int count = len / sizeof(ConversationSummary);
				if (count > 100)
					count = 100;
				memcpy(app.conversations, payload, count * sizeof(ConversationSummary));
				app.conv_count = count;
				pthread_mutex_unlock(&app.state_lock);
			}

			app.needs_redraw = 1; // TRIGGER REDRAW
			break;
		}
		case MSG_CONV_CREATED:
		{
			send_packet(app.sock_fd, MSG_REQ_CONVERSATIONS, NULL, 0);
			log_print(LOG_INFO, "Conversation creation sent successfully");
			app.needs_redraw = 1; // TRIGGER REDRAW
			break;
		}
		case MSG_RTE_TEXT:
		{
			RoutedMessagePayload *p = (RoutedMessagePayload *)payload;
			pthread_mutex_lock(&app.state_lock);

			if (app.current_state == STATE_CHAT && app.current_conv_id == p->conv_id)
			{
				char line[1200];
				snprintf(line, sizeof(line), "[%s]: %s\n", p->sender_username, p->text);
				strcat(app.chat_history, line);
				ui_draw_chat(app.current_conv_name, app.chat_history, app.chat_input_buffer);
			}
			else
			{
				for (int i = 0; i < app.conv_count; i++)
				{
					if (app.conversations[i].conv_id == p->conv_id)
					{
						app.conversations[i].unread_count++;
						app.needs_redraw = 1; // TRIGGER REDRAW
						break;
					}
				}
			}
			pthread_mutex_unlock(&app.state_lock);
			app.needs_redraw = 1; // TRIGGER REDRAW
			break;
		}
		case MSG_RESP_HISTORY:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				snprintf(app.chat_history, 16384, "%s", (char *)payload);
				pthread_mutex_unlock(&app.state_lock);
				if (app.current_state == STATE_CHAT)
					ui_draw_chat(app.current_conv_name, app.chat_history, app.chat_input_buffer);

				app.needs_redraw = 1; // TRIGGER REDRAW
			}
			break;
		}
		case MSG_RESP_CONTACTS:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				int count = len / sizeof(ContactSummary);
				if (count > 100)
					count = 100;
				memcpy(app.contacts, payload, count * sizeof(ContactSummary));
				app.contacts_count = count;
				pthread_mutex_unlock(&app.state_lock);
				app.needs_redraw = 1; // TRIGGER REDRAW
			}
			break;
		}
		case MSG_RESP_REQUESTS:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				int count = len / sizeof(ContactSummary);
				if (count > 100)
					count = 100;
				memcpy(app.requests, payload, count * sizeof(ContactSummary));
				app.requests_count = count;
				pthread_mutex_unlock(&app.state_lock);
				app.needs_redraw = 1; // TRIGGER REDRAW
			}
			break;
		}
		case MSG_RESP_MEMBERS:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				int count = len / sizeof(GroupMemberSummary);
				if (count > MAX_MEMBERS)
					count = MAX_MEMBERS;
				memcpy(app.current_group_members, payload, count * sizeof(GroupMemberSummary));
				app.current_group_members_count = count;
				pthread_mutex_unlock(&app.state_lock);
				app.needs_redraw = 1;
			}
			break;
		}
		case MSG_ADD_REQ_SENT:
			log_print(LOG_INFO, "Friend request sent successfully");
			app.needs_redraw = 1; // TRIGGER REDRAW
			break;
		case MSG_ADD_SUCCESS:
			send_packet(app.sock_fd, MSG_REQ_CONTACTS, NULL, 0);
			app.needs_redraw = 1; // TRIGGER REDRAW
			break;
		default:
			break;
		}
		if (payload)
			free(payload);
	}
	return NULL;
}

int main()
{
	log_init("client");
	log_print(LOG_INFO, "Client started");
	app_init();

	struct sockaddr_in serv_addr;
	if ((app.sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

	if (connect(app.sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		log_print(LOG_ERROR, "Connection failed to 127.0.0.1:%d", PORT);
		printf("Connection failed.\n");
		return -1;
	}
	log_print(LOG_INFO, "Connected to server");

	ui_init();

	// --- AUTH ---
	int authenticated = 0;
	LoginPayload session;
	if (load_session(&session))
	{
		send_packet(app.sock_fd, MSG_LOGIN, &session, sizeof(session));
		MessageType type;
		void *p;
		uint32_t len;
		recv_packet(app.sock_fd, &type, &p, &len);
		if (type == MSG_LOGIN_SUCCESS)
		{
			if (p)
				memcpy(&app.my_info, p, sizeof(MyInfoPayload));
			authenticated = 1;
		}
		else
			clear_session();
		if (p)
			free(p);
	}

	while (!authenticated)
	{
		int choice = ui_draw_start_menu();
		if (choice == 3)
		{
			ui_cleanup();
			log_close();
			return 0;
		}
		if (choice == 2)
		{
			RegisterPayload reg = {0};
			ui_draw_register(reg.email, reg.username, reg.password);
			send_packet(app.sock_fd, MSG_REGISTER, &reg, sizeof(reg));
			MessageType type;
			void *p;
			uint32_t len;
			recv_packet(app.sock_fd, &type, &p, &len);
			if (p)
				free(p);
		}
		if (choice == 1)
		{
			LoginPayload log = {0};
			int remember = 0; // Capture user preference

			// Pass address of remember variable
			ui_draw_login(log.email, log.password, &remember);

			send_packet(app.sock_fd, MSG_LOGIN, &log, sizeof(log));

			MessageType type;
			void *p;
			uint32_t len;
			recv_packet(app.sock_fd, &type, &p, &len);

			if (type == MSG_LOGIN_SUCCESS)
			{
				if (p)
					memcpy(&app.my_info, p, sizeof(MyInfoPayload));
				authenticated = 1;

				// Logic: Only save if user checked "Remember Me"
				if (remember)
				{
					save_session(log.email, log.password);
				}
				else
				{
					// Safety: Ensure no stale session exists if they chose NOT to be remembered
					clear_session();
				}
			}
			if (p)
				free(p);
		}
	}

	// --- AFTER AUTHENTICATION ---
	// 1. Switch Log File to contain User ID
	char log_name[64];
	snprintf(log_name, sizeof(log_name), "client_u%d", app.my_info.uid);
	log_update_name(log_name);

	// 2. Log User & System Information
	log_print(LOG_INFO, "=== SESSION START ===");
	log_print(LOG_INFO, "User: %s (ID: %d)", app.my_info.username, app.my_info.uid);
	log_print(LOG_INFO, "Email: %s", app.my_info.email);

	// 3. Get Machine Info
	struct utsname buffer;
	if (uname(&buffer) == 0)
	{
		log_print(LOG_INFO, "System: %s %s (%s)", buffer.sysname, buffer.release, buffer.machine);
	}

	char hostname[128];
	if (gethostname(hostname, sizeof(hostname)) == 0)
	{
		log_print(LOG_INFO, "Hostname: %s", hostname);
	}

	log_print(LOG_INFO, "=====================");

	// --- MAIN APP ---
	pthread_t th;
	pthread_create(&th, NULL, network_thread, NULL);
	app.needs_redraw = 1;

	app.current_state = STATE_HOME;
	send_packet(app.sock_fd, MSG_REQ_CONTACTS, NULL, 0);
	send_packet(app.sock_fd, MSG_GET_REQUESTS, NULL, 0);
	send_packet(app.sock_fd, MSG_REQ_CONVERSATIONS, NULL, 0);

	int selection = 0;

	// Global or static variables to hold group creation state
	int group_selection_idx = 0;
	int group_members_selected[100] = {0}; // Maps to app.contacts
	char group_name_buf[32] = "New Group";
	char group_desc_buf[64] = "Description";

	while (1)
	{
		// 1. RENDER
		if (app.needs_redraw)
		{
			if (app.current_state == STATE_HOME)
			{
				ui_draw_home_conversations(app.conversations, app.conv_count, selection);
			}
			else if (app.current_state == STATE_SETTINGS)
			{
				ui_draw_settings(&app.my_info);
			}
			else if (app.current_state == STATE_FRIENDS)
			{
				ui_draw_friends_list(app.contacts, app.contacts_count, selection, app.requests_count);
			}
			else if (app.current_state == STATE_CHAT)
			{
				ui_draw_chat(app.current_conv_name, app.chat_history, app.chat_input_buffer);
			}
			else if (app.current_state == STATE_REQUESTS)
			{
				ui_draw_requests(app.requests, app.requests_count, selection);
			}
			else if (app.current_state == STATE_CREATE_GROUP)
			{
				ui_draw_create_group_form(group_name_buf, group_desc_buf, app.contacts, app.contacts_count, group_members_selected, group_selection_idx);
			}
			else if (app.current_state == STATE_GROUP_SETTINGS)
			{
				ConversationSummary *c = NULL;
				for (int i = 0; i < app.conv_count; i++)
				{
					if (app.conversations[i].conv_id == app.current_conv_id)
					{
						c = &app.conversations[i];
						break;
					}
				}

				// Safety check: if conversation was deleted or not found, go back Home
				if (c)
				{
					ui_draw_group_settings(c);
				}
				else
				{
					app.current_state = STATE_HOME;
					app.needs_redraw = 1;
				}
			}

			else if (app.current_state == STATE_MANAGE_MEMBERS)
			{
				// Find if I am admin
				int is_admin = 0;
				for (int i = 0; i < app.conv_count; i++)
				{
					if (app.conversations[i].conv_id == app.current_conv_id)
					{
						is_admin = (app.conversations[i].my_role == 1);
						break;
					}
				}
				ui_draw_group_members(app.current_group_members, app.current_group_members_count, app.member_selection_idx, is_admin);
			}

			app.needs_redraw = 0; // Clear flag
		}

		// 2. INPUT
		int ch = getch();
		if (ch == ERR)
			continue;

		// 3. LOGIC
		app.needs_redraw = 1; // User input means UI likely changes
		if (app.current_state == STATE_HOME)
		{
			if (ch == 'q')
				break;
			if (ch == KEY_DOWN && selection < app.conv_count - 1)
				selection++;
			if (ch == KEY_UP && selection > 0)
				selection--;

			if (ch == 'g' || ch == 'G')
			{
				app.current_state = STATE_CREATE_GROUP;
				group_selection_idx = 0;
				memset(group_members_selected, 0, sizeof(group_members_selected));
				strcpy(group_name_buf, "My Group");
				strcpy(group_desc_buf, "");
			}

			if (ch == 'f' || ch == 'F')
			{
				app.current_state = STATE_FRIENDS;
				selection = 0;
				// Refresh data to be sure
				send_packet(app.sock_fd, MSG_REQ_CONTACTS, NULL, 0);
				send_packet(app.sock_fd, MSG_GET_REQUESTS, NULL, 0);
			}

			if (ch == 10 || ch == 13)
			{
				if (app.conv_count > 0)
				{
					app.current_conv_id = app.conversations[selection].conv_id;
					strncpy(app.current_conv_name, app.conversations[selection].name, 32);
					app.chat_history[0] = '\0';
					app.conversations[selection].unread_count = 0;

					app.current_state = STATE_CHAT;
					RequestHistoryPayload hp;
					hp.conv_id = app.current_conv_id;
					send_packet(app.sock_fd, MSG_REQ_HISTORY, &hp, sizeof(hp));
				}
			}

			if (ch == 's' || ch == 'S')
				app.current_state = STATE_SETTINGS;
		}
		else if (app.current_state == STATE_CHAT)
		{
			if (ch != ERR)
			{
				if (ch == 27)
				{
					app.current_state = STATE_HOME;
					app.chat_input_buffer[0] = '\0';
					send_packet(app.sock_fd, MSG_REQ_CONVERSATIONS, NULL, 0);
				}
				else if (ch == 10 || ch == 13)
				{
					if (strlen(app.chat_input_buffer) > 0)
					{
						SendMessagePayload msg;
						msg.conv_id = app.current_conv_id;
						strncpy(msg.text, app.chat_input_buffer, MAX_TEXT_LEN);
						send_packet(app.sock_fd, MSG_SEND_TEXT, &msg, sizeof(msg));

						char line[1200];
						snprintf(line, sizeof(line), "Me: %s\n", app.chat_input_buffer);
						pthread_mutex_lock(&app.state_lock);
						strcat(app.chat_history, line);
						pthread_mutex_unlock(&app.state_lock);
						app.chat_input_buffer[0] = '\0';
					}
				}
				else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
				{
					int len = strlen(app.chat_input_buffer);
					if (len > 0)
						app.chat_input_buffer[len - 1] = '\0';
				}
				else if (ch == KEY_F(1))
				{
					// Find current conversation object to get details/role
					for (int i = 0; i < app.conv_count; i++)
					{
						if (app.conversations[i].conv_id == app.current_conv_id)
						{
							// Only makes sense for Groups
							if (app.conversations[i].type == 1)
							{
								app.current_state = STATE_GROUP_SETTINGS;
							}
							break;
						}
					}
				}
				else if (ch >= 32 && ch <= 126)
				{
					int len = strlen(app.chat_input_buffer);
					if (len < MAX_TEXT_LEN - 1)
					{
						app.chat_input_buffer[len] = (char)ch;
						app.chat_input_buffer[len + 1] = '\0';
					}
				}
			}
		}
		else if (app.current_state == STATE_CREATE_GROUP)
		{
			if (ch == 27)
				app.current_state = STATE_HOME;

			if (ch == KEY_DOWN && group_selection_idx < app.contacts_count - 1)
				group_selection_idx++;
			if (ch == KEY_UP && group_selection_idx > 0)
				group_selection_idx--;

			if (ch == ' ')
			{
				group_members_selected[group_selection_idx] = !group_members_selected[group_selection_idx];
			}

			if (ch == 'n' || ch == 'N')
			{
				ui_input_string(3, 16, "Group Name: ", group_name_buf, 31);
			}

			if (ch == 'd' || ch == 'D')
			{
				ui_input_string(4, 17, "Group Description: ", group_desc_buf, 63);
			}

			if (ch == 10 || ch == 13)
			{
				CreateConvPayload ccp = {0};
				ccp.type = CONV_TYPE_GROUP;
				strncpy(ccp.name, group_name_buf, 31);
				strncpy(ccp.description, group_desc_buf, 63);

				ccp.participant_uids[0] = app.my_info.uid; // Add self
				int count = 1;

				for (int i = 0; i < app.contacts_count; i++)
				{
					if (group_members_selected[i] && count < MAX_PARTICIPANTS)
					{
						ccp.participant_uids[count++] = app.contacts[i].uid;
					}
				}
				ccp.participants_count = count;

				if (count > 1)
				{
					send_packet(app.sock_fd, MSG_CREATE_CONV, &ccp, sizeof(ccp));
					app.current_state = STATE_HOME;
				}
			}
		}
		else if (app.current_state == STATE_FRIENDS)
		{
			if (ch == 'b' || ch == 27 || ch == KEY_BACKSPACE)
			{
				app.current_state = STATE_HOME;
				selection = 0;
			}

			if (ch == KEY_DOWN && selection < app.contacts_count - 1)
				selection++;
			if (ch == KEY_UP && selection > 0)
				selection--;

			// [A] Add Friend
			if (ch == 'a' || ch == 'A')
			{
				char code[16];
				ui_prompt_friend_code(code);
				AddContactPayload acp;
				strncpy(acp.friend_code, code, 7);
				send_packet(app.sock_fd, MSG_ADD_BY_CODE, &acp, sizeof(acp));
			}

			// [R] Requests
			if (ch == 'r' || ch == 'R')
			{
				app.current_state = STATE_REQUESTS;
				selection = 0;
			}

			// [Enter] Message (Start/Open Private Chat)
			if (ch == 10 || ch == 13)
			{
				if (app.contacts_count > 0)
				{
					CreateConvPayload ccp = {0};
					strncpy(ccp.name, app.contacts[selection].username, 32);
					ccp.participants_count = 2;
					ccp.participant_uids[0] = app.my_info.uid;
					ccp.participant_uids[1] = app.contacts[selection].uid;
					ccp.type = CONV_TYPE_PRIVATE;
					send_packet(app.sock_fd, MSG_CREATE_CONV, &ccp, sizeof(ccp));

					// Go back to home to see the new conversation, or wait for it
					app.current_state = STATE_HOME;
				}
			}
		}
		else if (app.current_state == STATE_SETTINGS)
		{
			ui_draw_settings(&app.my_info);
			if (ch == 'l' || ch == 'L')
			{
				clear_session();
				break;
			}
			if (ch == 'e' || ch == 'E')
			{
				// Use ui_input_string for correct window handling
				UpdateUserPayload up = {0};

				pthread_mutex_lock(&app.state_lock);

				ui_input_string(10, 4, "New Username (leave empty for no update): ", up.new_username, 31);
				ui_input_string(12, 4, "New Password (leave empty for no update): ", up.new_password, 63);

				if (strlen(up.new_username) > 0 || strlen(up.new_password) > 0)
					send_packet(app.sock_fd, MSG_UPDATE_USER, &up, sizeof(up));

				// Force redraw next loop
			}
			if (ch == KEY_BACKSPACE || ch == 127 || ch == 'b')
				app.current_state = STATE_HOME;
		}
		else if (app.current_state == STATE_REQUESTS)
		{
			ui_draw_requests(app.requests, app.requests_count, selection);
			if (ch == 'b' || ch == KEY_BACKSPACE)
			{
				app.current_state = STATE_FRIENDS;
				selection = 0;
			}

			if (ch == 10 || ch == 'd')
			{
				if (app.requests_count > 0)
				{
					DecideRequestPayload p;
					// Use UID!
					p.target_uid = app.requests[selection].uid;
					p.accepted = (ch == 10);
					send_packet(app.sock_fd, MSG_DECIDE_REQUEST, &p, sizeof(p));
					app_remove_request(selection);
				}
			}
		}
		else if (app.current_state == STATE_GROUP_SETTINGS)
		{
			// Find current conversation data again
			ConversationSummary *c = NULL;
			for (int i = 0; i < app.conv_count; i++)
			{
				if (app.conversations[i].conv_id == app.current_conv_id)
				{
					c = &app.conversations[i];
					break;
				}
			}
			if (!c)
			{
				app.current_state = STATE_HOME;
				continue;
			}

			ui_draw_group_settings(c);

			if (ch == 27 || ch == KEY_BACKSPACE)
				app.current_state = STATE_CHAT;

			if (c->my_role == ROLE_ADMIN)
			{
				if (ch == KEY_DC || ch == 330)
				{ // Delete Key (check curses key code, often KEY_DC)
					DeleteGroupPayload p = {.conv_id = c->conv_id};
					send_packet(app.sock_fd, MSG_DELETE_GROUP, &p, sizeof(p));
					app.current_state = STATE_HOME;
				}
				if (ch == 'm' || ch == 'M')
				{
					app.current_state = STATE_MANAGE_MEMBERS;
					app.member_selection_idx = 0;
					// Request updated list
					ReqMembersPayload p = {.conv_id = c->conv_id};
					send_packet(app.sock_fd, MSG_REQ_MEMBERS, &p, sizeof(p));
				}
			}
			else
			{
				// Member view
				if (ch == 'm' || ch == 'M')
				{
					app.current_state = STATE_MANAGE_MEMBERS;
					ReqMembersPayload p = {.conv_id = c->conv_id};
					send_packet(app.sock_fd, MSG_REQ_MEMBERS, &p, sizeof(p));
				}
			}
		}
		else if (app.current_state == STATE_MANAGE_MEMBERS)
		{
			if (ch == 27)
				app.current_state = STATE_GROUP_SETTINGS;
			if (ch == KEY_DOWN && app.member_selection_idx < app.current_group_members_count - 1)
				app.member_selection_idx++;
			if (ch == KEY_UP && app.member_selection_idx > 0)
				app.member_selection_idx--;

			// Kick Logic
			if ((ch == 'k' || ch == 'K'))
			{
				// Check Admin rights again
				int is_admin = 0;
				for (int i = 0; i < app.conv_count; i++)
					if (app.conversations[i].conv_id == app.current_conv_id)
						is_admin = (app.conversations[i].my_role == 1);

				if (is_admin && app.current_group_members_count > 0)
				{
					uint32_t target = app.current_group_members[app.member_selection_idx].uid;
					// Don't kick self
					if (target != app.my_info.uid)
					{
						KickMemberPayload p;
						p.conv_id = app.current_conv_id;
						p.target_uid = target;
						send_packet(app.sock_fd, MSG_KICK_MEMBER, &p, sizeof(p));

						// Refresh list
						ReqMembersPayload rp = {.conv_id = app.current_conv_id};
						send_packet(app.sock_fd, MSG_REQ_MEMBERS, &rp, sizeof(rp));
					}
				}
			}
		}
	}

	ui_cleanup();
	log_print(LOG_INFO, "Client shutting down");
	log_close();
	close(app.sock_fd);
	return 0;
}