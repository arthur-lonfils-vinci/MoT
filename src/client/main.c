#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/stat.h>
#include <sys/types.h>

// Dynamic include at compilation
#include "server_cert.h"

// Infrastructure
#include "infrastructure/ui.h"
#include "infrastructure/client_context.h"

// System
#include "system/logger.h"
#include "system/protocol.h"
#include "system/config_loader.h"

// Services
#include "services/auth_service.h"
#include "services/chat_service.h"
#include "services/group_service.h"
#include "services/contact_service.h"

// --- Helper Functions ---

void check_and_notify(MessageType type, void *payload)
{
	if (type == MSG_RTE_TEXT)
	{
		RoutedMessagePayload *p = (RoutedMessagePayload *)payload;
		// Don't notify if we are strictly focusing on this chat
		if (app.current_state == STATE_DASHBOARD && app.current_conv_id == p->conv_id)
		{
			return;
		}
		char title[64];
		snprintf(title, sizeof(title), "Message from %s", p->sender_username);

		app.pending_notify = 1;
		strncpy(app.notify_title, title, 63);
		strncpy(app.notify_msg, p->text, 127);
	}
}

// Thread-safe history appender
static void safe_append_history(const char *line)
{
	size_t line_len = strlen(line);
	size_t current_len = app.chat_history ? app.chat_history_len : 0;

	char *new_hist = realloc(app.chat_history, current_len + line_len + 1);
	if (new_hist)
	{
		app.chat_history = new_hist;
		if (current_len == 0)
			app.chat_history[0] = '\0';
		strcat(app.chat_history, line);
		app.chat_history_len += line_len;
	}
}

// --- Network Thread ---
void *network_thread(void *arg)
{
	(void)arg;
	MessageType type;
	void *payload;
	uint32_t len;

	while (recv_packet(app.ssl, &type, &payload, &len) > 0)
	{
		switch (type)
		{
		case MSG_RESP_CONVERSATIONS:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				if (app.conversations)
					free(app.conversations);
				app.conversations = (ConversationSummary *)payload;
				app.conv_count = len / sizeof(ConversationSummary);
				payload = NULL;
				pthread_mutex_unlock(&app.state_lock);
			}
			app.needs_redraw = 1;
			break;
		}
		case MSG_CONV_CREATED:
		{
			service_req_conversations();
			app.needs_redraw = 1;
			break;
		}
		case MSG_RTE_TEXT:
		{
			RoutedMessagePayload *p = (RoutedMessagePayload *)payload;
			check_and_notify(MSG_RTE_TEXT, p);

			pthread_mutex_lock(&app.state_lock);

			if (app.current_state == STATE_DASHBOARD && app.current_conv_id == p->conv_id)
			{
				char line[1200];
				snprintf(line, sizeof(line), "[%s]: %s\n", p->sender_username, p->text);
				safe_append_history(line);
			}
			else
			{
				for (int i = 0; i < app.conv_count; i++)
				{
					if (app.conversations[i].conv_id == p->conv_id)
					{
						app.conversations[i].unread_count++;
						app.needs_redraw = 1;
						break;
					}
				}
			}
			pthread_mutex_unlock(&app.state_lock);
			app.needs_redraw = 1;
			break;
		}
		case MSG_RESP_HISTORY:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				if (app.chat_history)
					free(app.chat_history);

				app.chat_history = (char *)payload;
				app.chat_history_len = len;

				payload = NULL;
				pthread_mutex_unlock(&app.state_lock);
				app.needs_redraw = 1;
			}
			break;
		}
		case MSG_RESP_CONTACTS:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				if (app.contacts)
					free(app.contacts);
				app.contacts = (ContactSummary *)payload;
				app.contacts_count = len / sizeof(ContactSummary);
				payload = NULL;
				pthread_mutex_unlock(&app.state_lock);
				app.needs_redraw = 1;
			}
			break;
		}
		case MSG_RESP_REQUESTS:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				if (app.requests)
					free(app.requests);
				app.requests = (ContactSummary *)payload;
				app.requests_count = len / sizeof(ContactSummary);
				payload = NULL;
				pthread_mutex_unlock(&app.state_lock);
				app.needs_redraw = 1;
			}
			break;
		}
		case MSG_RESP_MEMBERS:
		{
			if (payload)
			{
				pthread_mutex_lock(&app.state_lock);
				if (app.current_group_members)
					free(app.current_group_members);
				app.current_group_members = (GroupMemberSummary *)payload;
				app.current_group_members_count = len / sizeof(GroupMemberSummary);
				payload = NULL;
				pthread_mutex_unlock(&app.state_lock);
				app.needs_redraw = 1;
			}
			break;
		}
		case MSG_ADD_REQ_SENT:
			app.needs_redraw = 1;
			break;
		case MSG_ADD_SUCCESS:
			service_refresh_contacts();
			break;
		default:
			break;
		}
		if (payload)
			free(payload);
	}
	return NULL;
}

// --- Main Application Loop ---

int main(int argc, char *argv[])
{
	log_init("client");
	log_print(LOG_INFO, "Client started");
	app_init(); // Initialize context

	// 1. CONFIGURATION
	AppConfig config;
	char config_path[512];
	config_get_client_path(config_path, sizeof(config_path));

	struct stat st;
	int first_run = (stat(config_path, &st) != 0);

	ui_init(); // Start Ncurses & Notify

	if (first_run)
	{
		char *last_slash = strrchr(config_path, '/');
		if (last_slash)
		{
			*last_slash = '\0';
			mkdir(config_path, 0700);
			*last_slash = '/';
		}

		apply_defaults(&config);
		ui_draw_first_start_wizard(config.server_host, sizeof(config.server_host), &config.port);
		config_save_client(config_path, &config);
		log_print(LOG_INFO, "First run configuration saved to %s", config_path);
	}
	else
	{
		config_load(config_path, &config);
		log_print(LOG_INFO, "Loaded configuration from %s", config_path);
	}

	if (argc > 1)
		strncpy(config.server_host, argv[1], sizeof(config.server_host) - 1);

	// 2. SSL INIT
	init_openssl();
	app.ctx = create_context(0);

	int cert_loaded = 0;
	if (strlen(config.ca_cert_path) > 0)
	{
		if (SSL_CTX_load_verify_locations(app.ctx, config.ca_cert_path, NULL))
			cert_loaded = 1;
	}
	if (!cert_loaded)
	{
		if (stat("server.crt", &st) == 0)
		{
			if (SSL_CTX_load_verify_locations(app.ctx, "server.crt", NULL))
				cert_loaded = 1;
		}
	}
	if (!cert_loaded)
	{
		if (load_cert_from_memory(app.ctx, SERVER_CERT_PEM))
			cert_loaded = 1;
	}

	if (cert_loaded)
	{
		SSL_CTX_set_verify(app.ctx, SSL_VERIFY_PEER, NULL);
	}

	// 3. CONNECTION
	struct addrinfo hints, *res, *p;
	char port_str[6];
	snprintf(port_str, sizeof(port_str), "%d", config.port);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(config.server_host, port_str, &hints, &res) != 0)
	{
		ui_cleanup();
		cleanup_openssl();
		return -1;
	}

	app.sock_fd = -1;
	for (p = res; p != NULL; p = p->ai_next)
	{
		if ((app.sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			continue;
		if (connect(app.sock_fd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(app.sock_fd);
			app.sock_fd = -1;
			continue;
		}
		break;
	}
	freeaddrinfo(res);

	if (app.sock_fd == -1)
	{
		ui_cleanup();
		cleanup_openssl();
		return -1;
	}

	app.ssl = SSL_new(app.ctx);
	SSL_set_fd(app.ssl, app.sock_fd);

	if (SSL_connect(app.ssl) <= 0)
	{
		close(app.sock_fd);
		SSL_free(app.ssl);
		SSL_CTX_free(app.ctx);
		ui_cleanup();
		cleanup_openssl();
		return -1;
	}

	// 4. AUTHENTICATION LOOP
	int authenticated = 0;
	LoginPayload session;

	if (load_session(&session))
	{
		if (service_login(session.email, session.password))
		{
			authenticated = 1;
		}
		else
		{
			clear_session();
		}
	}

	while (!authenticated)
	{
		int choice = ui_draw_start_menu();
		if (choice == 3)
		{ // Quit
			ui_cleanup();
			app_cleanup();
			log_close();
			SSL_shutdown(app.ssl);
			SSL_free(app.ssl);
			SSL_CTX_free(app.ctx);
			close(app.sock_fd);
			cleanup_openssl();
			return 0;
		}
		if (choice == 2)
		{
			RegisterPayload reg = {0};
			ui_draw_register(reg.email, reg.username, reg.password);
			service_register(reg.email, reg.username, reg.password);
		}
		if (choice == 1)
		{
			LoginPayload log = {0};
			int remember = 0;
			ui_draw_login(log.email, log.password, &remember);

			if (service_login(log.email, log.password))
			{
				authenticated = 1;
				if (remember)
					save_session(log.email, log.password);
				else
					clear_session();
			}
		}
	}

	// 5. SESSION START
	char log_name[64];
	snprintf(log_name, sizeof(log_name), "client_u%d", app.my_info.uid);
	log_update_name(log_name);

	pthread_t th;
	pthread_create(&th, NULL, network_thread, NULL);

	app.needs_redraw = 1;
	app.current_state = STATE_DASHBOARD;

	// Fetch initial data
	service_refresh_contacts();
	service_refresh_requests();
	service_req_conversations();

	// Local state for navigation
	int selected_idx = 0;
	int group_selection_idx = 0;
	int group_members_selected[100] = {0};
	char group_name_buf[32] = "New Group";
	char group_desc_buf[64] = "Description";

	// 6. MAIN EVENT LOOP
	while (1)
	{
		// A. Notifications
		if (app.pending_notify)
		{
			ui_show_notification(app.notify_title, app.notify_msg);
			app.pending_notify = 0;
		}

		// B. Redraw
		if (app.needs_redraw)
		{
			if (app.current_state == STATE_DASHBOARD)
			{
				// Sync sidebar selection
				if (app.conv_count > 0 && selected_idx >= app.conv_count)
					selected_idx = app.conv_count - 1;
				if (app.conv_count == 0)
					selected_idx = 0;

				// Auto-switch conversation if selection changed
				if (app.conv_count > 0)
				{
					uint32_t target_id = app.conversations[selected_idx].conv_id;
					if (app.current_conv_id != target_id)
					{
						app.current_conv_id = target_id;
						strncpy(app.current_conv_name, app.conversations[selected_idx].name, 32);

						pthread_mutex_lock(&app.state_lock);
						if (app.chat_history)
						{
							free(app.chat_history);
							app.chat_history = NULL;
						}
						app.chat_history_len = 0;
						pthread_mutex_unlock(&app.state_lock);

						service_req_history(target_id);
						app.conversations[selected_idx].unread_count = 0;
					}
				}

				ui_refresh_dashboard(app.conversations, app.conv_count, selected_idx,
														 app.chat_history, app.chat_input_buffer);
			}
			else if (app.current_state == STATE_SETTINGS)
			{
				ui_draw_settings(&app.my_info);
			}
			else if (app.current_state == STATE_FRIENDS)
			{
				ui_draw_friends_list(app.contacts, app.contacts_count, selected_idx, app.requests_count);
			}
			else if (app.current_state == STATE_REQUESTS)
			{
				ui_draw_requests(app.requests, app.requests_count, selected_idx);
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
				if (c)
					ui_draw_group_settings(c);
				else
					app.current_state = STATE_DASHBOARD;
			}
			else if (app.current_state == STATE_MANAGE_MEMBERS)
			{
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

			app.needs_redraw = 0;
		}

		// C. Input Handling
		int ch = wgetch(stdscr);
		if (ch == ERR)
			continue;

		app.needs_redraw = 1;

		// Dispatch based on State
		if (app.current_state == STATE_DASHBOARD)
		{
			if (ch == KEY_MOUSE)
			{
				int clicked_idx = -1;
				int action = ui_handle_mouse_dashboard(&clicked_idx);
				if (action == 1)
					selected_idx = clicked_idx;
				if (action == 4)
					ch = KEY_F(1); // F1 Clicked
				if (action == 5)
					ch = KEY_F(2); // F2 Clicked
				if (action == 6)
					ch = KEY_F(3); // F3 Clicked
			}
			else if (ch == KEY_UP && selected_idx > 0)
				selected_idx--;
			else if (ch == KEY_DOWN && selected_idx < app.conv_count - 1)
				selected_idx++;

			// --- SHORTCUTS ---
			if (ch == KEY_F(1))
			{
				app.current_state = STATE_FRIENDS;
				selected_idx = 0;
				service_refresh_contacts();
			}
			else if (ch == KEY_F(2))
			{
				app.current_state = STATE_SETTINGS;
			}
			else if (ch == KEY_F(3))
			{
				app.current_state = STATE_CREATE_GROUP;
				group_selection_idx = 0;
				memset(group_members_selected, 0, sizeof(group_members_selected));
			}

			// Chat Input
			else if (ch == '\n' || ch == KEY_ENTER)
			{
				if (strlen(app.chat_input_buffer) > 0)
				{
					service_send_text(app.current_conv_id, app.chat_input_buffer);
					char line[1200];
					snprintf(line, sizeof(line), "Me: %s\n", app.chat_input_buffer);
					pthread_mutex_lock(&app.state_lock);
					safe_append_history(line);
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
			else if (ch >= 32 && ch <= 126)
			{
				int len = strlen(app.chat_input_buffer);
				if (len < MAX_TEXT_LEN - 1)
				{
					app.chat_input_buffer[len] = ch;
					app.chat_input_buffer[len + 1] = '\0';
				}
			}
			else if (ch == 27)
			{ // ESC exits if typing empty
				if (strlen(app.chat_input_buffer) == 0)
					break;
			}
		}
		else if (app.current_state == STATE_FRIENDS)
		{
			if (ch == 27 || ch == KEY_BACKSPACE)
				app.current_state = STATE_DASHBOARD;
			else if (ch == KEY_DOWN && selected_idx < app.contacts_count - 1)
				selected_idx++;
			else if (ch == KEY_UP && selected_idx > 0)
				selected_idx--;
			else if (ch == 'a' || ch == 'A')
			{
				char code[16];
				ui_prompt_friend_code(code);
				service_add_contact(code);
			}
			else if (ch == 'r' || ch == 'R')
			{
				app.current_state = STATE_REQUESTS;
				selected_idx = 0;
			}
			else if (ch == '\n' || ch == KEY_ENTER)
			{
				if (app.contacts_count > 0 && app.contacts)
				{
					service_create_private(app.contacts[selected_idx].uid, app.contacts[selected_idx].username);
					app.current_state = STATE_DASHBOARD;
				}
			}
		}
		else if (app.current_state == STATE_REQUESTS)
		{
			if (ch == 27 || ch == KEY_BACKSPACE)
				app.current_state = STATE_FRIENDS;
			else if (ch == KEY_DOWN && selected_idx < app.requests_count - 1)
				selected_idx++;
			else if (ch == KEY_UP && selected_idx > 0)
				selected_idx--;
			else if (ch == '\n' || ch == 'd')
			{
				if (app.requests_count > 0 && app.requests)
				{
					service_decide_request(app.requests[selected_idx].uid, (ch == '\n'));
					// Note: Requests list updates async via network thread, assume redraw will fix
				}
			}
		}
		else if (app.current_state == STATE_CREATE_GROUP)
		{
			if (ch == 27)
				app.current_state = STATE_DASHBOARD;
			else if (ch == KEY_DOWN && group_selection_idx < app.contacts_count - 1)
				group_selection_idx++;
			else if (ch == KEY_UP && group_selection_idx > 0)
				group_selection_idx--;
			else if (ch == ' ')
				group_members_selected[group_selection_idx] = !group_members_selected[group_selection_idx];
			else if (ch == 'n' || ch == 'N')
				ui_input_string(3, 16, "Group Name: ", group_name_buf, 31);
			else if (ch == 'd' || ch == 'D')
				ui_input_string(4, 17, "Description: ", group_desc_buf, 63);
			else if (ch == '\n' || ch == KEY_ENTER)
			{
				uint32_t selected_uids[MAX_PARTICIPANTS];
				int count = 0;
				for (int i = 0; i < app.contacts_count; i++)
				{
					if (group_members_selected[i])
						selected_uids[count++] = app.contacts[i].uid;
				}
				if (service_create_group(group_name_buf, group_desc_buf, selected_uids, count))
					app.current_state = STATE_DASHBOARD;
			}
		}
		else if (app.current_state == STATE_SETTINGS)
		{
			if (ch == 27 || ch == KEY_BACKSPACE)
				app.current_state = STATE_DASHBOARD;
			else if (ch == 'l' || ch == 'L')
			{
				service_logout();
				break;
			}
			else if (ch == 'e' || ch == 'E')
			{
				char u[32] = {0}, p[64] = {0};
				pthread_mutex_lock(&app.state_lock);
				ui_input_string(10, 4, "New Username: ", u, 31);
				ui_input_string(12, 4, "New Password: ", p, 63);
				pthread_mutex_unlock(&app.state_lock);
				if (strlen(u) > 0 || strlen(p) > 0)
					service_update_user(u, p);
			}
		}
		else if (app.current_state == STATE_GROUP_SETTINGS)
		{
			int is_admin = 0;
			for (int i = 0; i < app.conv_count; i++)
				if (app.conversations[i].conv_id == app.current_conv_id)
					is_admin = (app.conversations[i].my_role == 1);

			if (ch == 27 || ch == KEY_BACKSPACE)
				app.current_state = STATE_DASHBOARD;
			if (is_admin)
			{
				if (ch == KEY_DC || ch == 330)
				{
					service_delete_group(app.current_conv_id);
					app.current_state = STATE_DASHBOARD;
				}
			}
			if (ch == 'm' || ch == 'M')
			{
				app.current_state = STATE_MANAGE_MEMBERS;
				app.member_selection_idx = 0;
				service_req_members(app.current_conv_id);
			}
		}
		else if (app.current_state == STATE_MANAGE_MEMBERS)
		{
			if (ch == 27)
				app.current_state = STATE_GROUP_SETTINGS;
			else if (ch == KEY_DOWN && app.member_selection_idx < app.current_group_members_count - 1)
				app.member_selection_idx++;
			else if (ch == KEY_UP && app.member_selection_idx > 0)
				app.member_selection_idx--;
			else if (ch == 'k' || ch == 'K')
			{
				int is_admin = 0;
				for (int i = 0; i < app.conv_count; i++)
					if (app.conversations[i].conv_id == app.current_conv_id)
						is_admin = (app.conversations[i].my_role == 1);

				if (is_admin && app.current_group_members_count > 0)
				{
					uint32_t target = app.current_group_members[app.member_selection_idx].uid;
					if (target != app.my_info.uid)
					{
						service_kick_member(app.current_conv_id, target);
						service_req_members(app.current_conv_id);
					}
				}
			}
		}
	}

	// 7. CLEANUP
	ui_cleanup();
	app_cleanup();
	log_print(LOG_INFO, "Client shutting down");
	log_close();

	if (app.ssl)
	{
		SSL_shutdown(app.ssl);
		SSL_free(app.ssl);
	}
	if (app.ctx)
	{
		SSL_CTX_free(app.ctx);
	}
	if (app.sock_fd != -1)
	{
		close(app.sock_fd);
	}
	cleanup_openssl();

	return 0;
}