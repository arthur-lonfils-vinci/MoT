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

#include "server_cert.h"
#include "infrastructure/ui.h"
#include "infrastructure/client_context.h"
#include "infrastructure/theme.h"
#include "system/logger.h"
#include "system/protocol.h"
#include "system/config_loader.h"
#include "services/auth_service.h"
#include "services/chat_service.h"
#include "services/group_service.h"
#include "services/contact_service.h"

void check_and_notify(MessageType type, void *payload)
{
	if (type == MSG_RTE_TEXT)
	{
		RoutedMessagePayload *p = (RoutedMessagePayload *)payload;
		if (app.current_state == STATE_DASHBOARD && app.current_conv_id == p->conv_id)
			return;
		char title[64];
		snprintf(title, sizeof(title), "Message from %s", p->sender_username);
		app.pending_notify = 1;
		strncpy(app.notify_title, title, 63);
		strncpy(app.notify_msg, p->text, 127);
	}
}

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
			service_req_conversations();
			app.needs_redraw = 1;
			break;
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

int main(int argc, char *argv[])
{
	log_init("client");
	app_init();
	AppConfig config;
	char config_path[512];
	config_get_client_path(config_path, sizeof(config_path));
	struct stat st;
	int first_run = (stat(config_path, &st) != 0);
	ui_init();

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
	}
	else
	{
		config_load(config_path, &config);
		ui_theme_init(config.theme_id); // Apply Saved Theme
	}
	if (argc > 1)
		strncpy(config.server_host, argv[1], sizeof(config.server_host) - 1);

	init_openssl();
	app.ctx = create_context(0);
	int cert_loaded = 0;
	if (strlen(config.ca_cert_path) > 0)
		if (SSL_CTX_load_verify_locations(app.ctx, config.ca_cert_path, NULL))
			cert_loaded = 1;
	if (!cert_loaded && stat("server.crt", &st) == 0)
		if (SSL_CTX_load_verify_locations(app.ctx, "server.crt", NULL))
			cert_loaded = 1;
	if (!cert_loaded)
		if (load_cert_from_memory(app.ctx, SERVER_CERT_PEM))
			cert_loaded = 1;
	if (cert_loaded)
		SSL_CTX_set_verify(app.ctx, SSL_VERIFY_PEER, NULL);

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

	int authenticated = 0, remember_me = 0;
	LoginPayload session;
	if (load_session(&session))
	{
		if (service_login(session.email, session.password))
			authenticated = 1;
		else
			clear_session();
	}

	while (!authenticated)
	{
		int choice = ui_draw_start_menu();
		if (choice == 3)
		{
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
			if (ui_draw_register(reg.email, reg.username, reg.password))
				service_register(reg.email, reg.username, reg.password);
		}
		if (choice == 1)
		{
			LoginPayload log = {0};
			if (ui_draw_login(log.email, log.password, &remember_me))
			{
				if (service_login(log.email, log.password))
				{
					authenticated = 1;
					if (remember_me)
						save_session(log.email, log.password);
					else
						clear_session();
				}
			}
		}
	}

	char log_name[64];
	snprintf(log_name, sizeof(log_name), "client_u%d", app.my_info.uid);
	log_update_name(log_name);
	pthread_t th;
	pthread_create(&th, NULL, network_thread, NULL);
	app.needs_redraw = 1;
	app.current_state = STATE_DASHBOARD;
	service_refresh_contacts();
	service_refresh_requests();
	service_req_conversations();

	int selected_idx = 0;
	int group_selection_idx = 0;
	int group_members_selected[100] = {0};
	char group_name_buf[32] = "New Group";
	char group_desc_buf[64] = "Desc";

	while (1)
	{
		if (app.pending_notify)
		{
			ui_show_notification(app.notify_title, app.notify_msg);
			app.pending_notify = 0;
		}
		if (app.needs_redraw)
		{
			if (app.current_state == STATE_DASHBOARD)
			{
				if (app.conv_count > 0 && selected_idx >= app.conv_count)
					selected_idx = app.conv_count - 1;
				if (app.conv_count == 0)
					selected_idx = 0;
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
				ui_refresh_dashboard(app.conversations, app.conv_count, selected_idx, app.chat_history, app.chat_input_buffer);
			}
			else if (app.current_state == STATE_SETTINGS)
			{
				ui_draw_settings(&app.my_info);
			}
			else if (app.current_state == STATE_FRIENDS)
			{
				ui_draw_friends_list(app.contacts, app.contacts_count, &selected_idx, app.requests_count);
			}
			else if (app.current_state == STATE_REQUESTS)
			{
				ui_draw_requests(app.requests, app.requests_count, &selected_idx);
			}
			else if (app.current_state == STATE_CREATE_GROUP)
			{
				ui_draw_create_group_form(group_name_buf, group_desc_buf, app.contacts, app.contacts_count, group_members_selected, &group_selection_idx);
			}
			else if (app.current_state == STATE_GROUP_SETTINGS)
			{
				ConversationSummary *c = NULL;
				for (int i = 0; i < app.conv_count; i++)
					if (app.conversations[i].conv_id == app.current_conv_id)
					{
						c = &app.conversations[i];
						break;
					}
				if (!c)
				{
					app.current_state = STATE_DASHBOARD;
					continue;
				}
				ui_draw_group_settings(c);
			}
			else if (app.current_state == STATE_MANAGE_MEMBERS)
			{
				int is_admin = 0;
				for (int i = 0; i < app.conv_count; i++)
					if (app.conversations[i].conv_id == app.current_conv_id)
						is_admin = (app.conversations[i].my_role == 1);
				ui_draw_group_members(app.current_group_members, app.current_group_members_count, &app.member_selection_idx, is_admin);
			}
			app.needs_redraw = 0;
		}

		// Logic for Non-Blocking Dashboard vs Blocking Modals
		if (app.current_state == STATE_DASHBOARD)
		{
			int ch = wgetch(stdscr);
			if (ch == ERR)
				continue;
			app.needs_redraw = 1;
			if (ch == KEY_RESIZE)
			{
				ui_resize();
				continue;
			}

			if (ch == KEY_MOUSE)
			{
				int cidx = -1;
				int act = ui_handle_mouse_dashboard(&cidx);
				if (act == 1)
					selected_idx = cidx;
				else if (act == 4)
					ch = KEY_F(1);
				else if (act == 5)
					ch = KEY_F(2);
				else if (act == 6)
					ch = KEY_F(3);
				else if (act == 7)
					ch = KEY_F(4);
				else if (act == 9)
					break;
			}
			if (ch == KEY_UP && selected_idx > 0)
				selected_idx--;
			else if (ch == KEY_DOWN && selected_idx < app.conv_count - 1)
				selected_idx++;
			else if (ch == KEY_F(1))
			{
				app.current_state = STATE_FRIENDS;
				selected_idx = 0;
				service_refresh_contacts();
				service_refresh_requests();
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
			else if (ch == KEY_F(4))
			{
				if (app.conv_count > 0 && app.conversations[selected_idx].type == 1)
					app.current_state = STATE_GROUP_SETTINGS;
			}
			else if (ch == '\n')
			{
				if (strlen(app.chat_input_buffer) > 0)
				{
					service_send_text(app.current_conv_id, app.chat_input_buffer);
					char l[1200];
					snprintf(l, sizeof(l), "Me: %s\n", app.chat_input_buffer);
					pthread_mutex_lock(&app.state_lock);
					safe_append_history(l);
					pthread_mutex_unlock(&app.state_lock);
					app.chat_input_buffer[0] = 0;
				}
			}
			else if (ch == 27)
			{
				if (strlen(app.chat_input_buffer) == 0)
					break;
				else
					memset(app.chat_input_buffer, 0, sizeof(app.chat_input_buffer));
			}
			else if (ch == KEY_BACKSPACE || ch == 127)
			{
				int l = strlen(app.chat_input_buffer);
				if (l > 0)
					app.chat_input_buffer[l - 1] = 0;
			}
			else if (ch >= 32 && ch <= 126)
			{
				int l = strlen(app.chat_input_buffer);
				if (l < MAX_TEXT_LEN - 1)
				{
					app.chat_input_buffer[l] = ch;
					app.chat_input_buffer[l + 1] = 0;
				}
			}
		}
		else if (app.current_state == STATE_FRIENDS)
		{
			int ch = wgetch(stdscr);
			if (ch == ERR)
				continue;
			app.needs_redraw = 1;
			if (ch == 27)
				app.current_state = STATE_DASHBOARD;
			else if (ch == KEY_DOWN && selected_idx < app.contacts_count - 1)
				selected_idx++;
			else if (ch == KEY_UP && selected_idx > 0)
				selected_idx--;
			else if (ch == 'a' || ch == 'A')
			{
				char c[16];
				ui_prompt_friend_code(c);
				service_add_contact(c);
			}
			else if (ch == 'r' || ch == 'R')
			{
				app.current_state = STATE_REQUESTS;
				selected_idx = 0;
			}
			else if (ch == '\n')
			{ // Chat
				if (app.contacts_count > 0)
				{
					service_create_private(app.contacts[selected_idx].uid, app.contacts[selected_idx].username);
					app.current_state = STATE_DASHBOARD;
				}
			}
		}
		else if (app.current_state == STATE_REQUESTS)
		{
			int ch = wgetch(stdscr);
			if (ch == ERR)
				continue;
			app.needs_redraw = 1;
			if (ch == 27)
				app.current_state = STATE_FRIENDS;
			else if (ch == KEY_DOWN && selected_idx < app.requests_count - 1)
				selected_idx++;
			else if (ch == KEY_UP && selected_idx > 0)
				selected_idx--;
			else if (ch == '\n')
			{ // Accept
				if (app.requests_count > 0)
				{
					service_decide_request(app.requests[selected_idx].uid, 1);
					service_refresh_requests();
					service_refresh_contacts();
					selected_idx = 0;
				}
			}
			else if (ch == 'd')
			{ // Deny
				if (app.requests_count > 0)
				{
					service_decide_request(app.requests[selected_idx].uid, 0);
					service_refresh_requests();
					service_refresh_contacts();
					selected_idx = 0;
				}
			}
		}
		else if (app.current_state == STATE_SETTINGS)
		{
			int ch = wgetch(stdscr);
			if (ch == ERR)
				continue;
			app.needs_redraw = 1;
			if (ch == 27)
				app.current_state = STATE_DASHBOARD;
			else if (ch == 'l' || ch == 'L')
			{
				service_logout();
				break;
			}
			else if (ch == 't' || ch == 'T')
			{
				ui_theme_cycle();
				config.theme_id = ui_theme_get_index();
				config_save_client(config_path, &config);
			}
			else if (ch == 'e' || ch == 'E')
			{
				char u[32] = {0}, p[64] = {0};
				ui_prompt_input("EDIT", "User:", u, 31);
				ui_prompt_input("EDIT", "Pass:", p, 63);
				if (strlen(u) > 0 || strlen(p) > 0)
					service_update_user(u, p);
			}
		}
		else if (app.current_state == STATE_CREATE_GROUP)
		{
			int ch = wgetch(stdscr);
			if (ch == ERR)
				continue;
			app.needs_redraw = 1;
			if (ch == 27)
				app.current_state = STATE_DASHBOARD;
			else if (ch == KEY_DOWN && group_selection_idx < app.contacts_count - 1)
				group_selection_idx++;
			else if (ch == KEY_UP && group_selection_idx > 0)
				group_selection_idx--;
			else if (ch == ' ')
				group_members_selected[group_selection_idx] = !group_members_selected[group_selection_idx];
			else if (ch == 'n' || ch == 'N')
				ui_prompt_input("GROUP", "Name:", group_name_buf, 31);
			else if (ch == 'd' || ch == 'D')
				ui_prompt_input("GROUP", "Desc:", group_desc_buf, 63);
			else if (ch == '\n')
			{
				uint32_t uids[100];
				int cnt = 0;
				for (int i = 0; i < app.contacts_count; i++)
					if (group_members_selected[i])
						uids[cnt++] = app.contacts[i].uid;
				if (service_create_group(group_name_buf, group_desc_buf, uids, cnt))
					app.current_state = STATE_DASHBOARD;
			}
		}
		else if (app.current_state == STATE_GROUP_SETTINGS)
		{
			int ch = wgetch(stdscr);
			if (ch == ERR)
				continue;
			app.needs_redraw = 1;
			if (ch == 27)
				app.current_state = STATE_DASHBOARD;
			else if (ch == 'm' || ch == 'M')
			{
				app.current_state = STATE_MANAGE_MEMBERS;
				app.member_selection_idx = 0;
				service_req_members(app.current_conv_id);
			}
			else if (ch == KEY_DC || ch == 'd')
			{
				int is_admin = 0;
				for (int i = 0; i < app.conv_count; i++)
					if (app.conversations[i].conv_id == app.current_conv_id)
						is_admin = (app.conversations[i].my_role == 1);
				if (is_admin)
				{
					service_delete_group(app.current_conv_id);
					app.current_state = STATE_DASHBOARD;
				}
			}
		}
		else if (app.current_state == STATE_MANAGE_MEMBERS)
		{
			int ch = wgetch(stdscr);
			if (ch == ERR)
				continue;
			app.needs_redraw = 1;
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
					service_kick_member(app.current_conv_id, app.current_group_members[app.member_selection_idx].uid);
					service_req_members(app.current_conv_id);
				}
			}
		}
	}

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