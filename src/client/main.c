#define _POSIX_C_SOURCE 200112L
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

// Dynamic include at compilation
#include "server_cert.h"

// Infrastructure
#include "infrastructure/ui.h"
#include "infrastructure/client_context.h"

// System
#include "system/logger.h"
#include "system/protocol.h"
#include <system/config_loader.h>

// Services
#include "services/auth_service.h"
#include "services/chat_service.h"
#include "services/group_service.h"
#include "services/contact_service.h"

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
				// Free old
				if (app.conversations)
					free(app.conversations);

				// Take ownership of payload
				app.conversations = (ConversationSummary *)payload;
				app.conv_count = len / sizeof(ConversationSummary);

				// Set payload to NULL so it's not freed at end of loop
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
			pthread_mutex_lock(&app.state_lock);

			if (app.current_state == STATE_CHAT && app.current_conv_id == p->conv_id)
			{
				char line[1200];
				snprintf(line, sizeof(line), "[%s]: %s\n", p->sender_username, p->text);
				app_append_history(line); // Use new helper
				ui_draw_chat(app.current_conv_name, app.chat_history ? app.chat_history : "", app.chat_input_buffer);
			}
			else
			{
				// Simple update unread check
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
				app.chat_history_len = len; // payload len for history string

				payload = NULL; // Take ownership
				pthread_mutex_unlock(&app.state_lock);

				if (app.current_state == STATE_CHAT)
					ui_draw_chat(app.current_conv_name, app.chat_history, app.chat_input_buffer);

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

				payload = NULL; // Take ownership
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

				payload = NULL; // Take ownership
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

				payload = NULL; // Take ownership
				pthread_mutex_unlock(&app.state_lock);
				app.needs_redraw = 1;
			}
			break;
		}
		case MSG_ADD_REQ_SENT:
			log_print(LOG_INFO, "Friend request sent successfully");
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
	log_print(LOG_INFO, "Client started");
	app_init();

	// 1. CONFIGURATION
	AppConfig config;
	char config_path[512];
	config_get_client_path(config_path, sizeof(config_path));

	// Check if config exists
	struct stat st;
	int first_run = (stat(config_path, &st) != 0);

	ui_init();

	if (first_run)
	{
		// Create directory if needed
		char *last_slash = strrchr(config_path, '/');
		if (last_slash)
		{
			*last_slash = '\0';
			mkdir(config_path, 0700); // Create ~/.mot
			*last_slash = '/';
		}

		apply_defaults(&config); // Pre-fill defaults
		ui_draw_first_start_wizard(config.server_host, sizeof(config.server_host), &config.port);
		config_save_client(config_path, &config);
		log_print(LOG_INFO, "First run configuration saved to %s", config_path);
	}
	else
	{
		config_load(config_path, &config);
		log_print(LOG_INFO, "Loaded configuration from %s", config_path);
	}

	// CLI Override (Optional)
	if (argc > 1)
		strncpy(config.server_host, argv[1], sizeof(config.server_host) - 1);

	// 2. SSL INIT (Updated)
	init_openssl();
	app.ctx = create_context(0);

	int cert_loaded = 0;

	// Certificate Logic: Custom CA vs Embedded
	// Priority 1: Config File Path
	if (strlen(config.ca_cert_path) > 0)
	{
		if (SSL_CTX_load_verify_locations(app.ctx, config.ca_cert_path, NULL))
		{
			log_print(LOG_INFO, "Loaded custom CA from config: %s", config.ca_cert_path);
			cert_loaded = 1;
		}
	}

	// Priority 2: Portable "server.crt" in current folder
	if (!cert_loaded)
	{
		struct stat st;
		if (stat("server.crt", &st) == 0)
		{
			if (SSL_CTX_load_verify_locations(app.ctx, "server.crt", NULL))
			{
				log_print(LOG_INFO, "Detected and loaded local 'server.crt'");
				cert_loaded = 1;
			}
		}
	}

	// Priority 3: Embedded Cert (Fallback)
	if (!cert_loaded)
	{
		if (load_cert_from_memory(app.ctx, SERVER_CERT_PEM))
		{
			log_print(LOG_INFO, "Loaded embedded server certificate.");
			cert_loaded = 1;
		}
	}

	if (cert_loaded)
	{
		SSL_CTX_set_verify(app.ctx, SSL_VERIFY_PEER, NULL);
	}

	// 3. CONNECTION (Use config)
	log_print(LOG_INFO, "Connecting to %s:%d...", config.server_host, config.port);

	struct addrinfo hints, *res, *p;
	char port_str[6];
	snprintf(port_str, sizeof(port_str), "%d", config.port);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(config.server_host, port_str, &hints, &res) != 0)
	{
		log_print(LOG_ERROR, "DNS lookup failed for %s", config.server_host);
		printf("Error: Could not resolve hostname %s\n", config.server_host);
		ui_cleanup();
		cleanup_openssl();
		return -1;
	}

	app.sock_fd = -1;
	printf("Connecting to %s:%d...\n", config.server_host, config.port);

	for (p = res; p != NULL; p = p->ai_next)
	{
		if ((app.sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			continue;
		}
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
		log_print(LOG_ERROR, "Failed to connect to %s", config.server_host);
		printf("Connection failed.\n");
		ui_cleanup();
		cleanup_openssl();
		return -1;
	}

	// 2. Perform SSL Handshake
	app.ssl = SSL_new(app.ctx);
	SSL_set_fd(app.ssl, app.sock_fd);

	if (SSL_connect(app.ssl) <= 0)
	{
		printf("SSL Handshake failed.\n");
		ERR_print_errors_fp(stderr);
		close(app.sock_fd);
		SSL_free(app.ssl);
		SSL_CTX_free(app.ctx);
		ui_cleanup();
		cleanup_openssl();
		return -1;
	}

	printf("Connected to server securely.\n");
	log_print(LOG_INFO, "Connected to server via SSL");

	// --- AUTH ---
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
			app_cleanup(); // CLEAN MEMORY
			log_close();
			SSL_shutdown(app.ssl);
			SSL_free(app.ssl);
			SSL_CTX_free(app.ctx);
			close(app.sock_fd);
			cleanup_openssl();
			return 0;
		}
		if (choice == 2)
		{ // Register
			RegisterPayload reg = {0};
			ui_draw_register(reg.email, reg.username, reg.password);
			service_register(reg.email, reg.username, reg.password);
		}
		if (choice == 1)
		{ // Login
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

	// --- AFTER AUTHENTICATION ---
	char log_name[64];
	snprintf(log_name, sizeof(log_name), "client_u%d", app.my_info.uid);
	log_update_name(log_name);

	log_print(LOG_INFO, "=== SESSION START ===");
	log_print(LOG_INFO, "User: %s (ID: %d)", app.my_info.username, app.my_info.uid);

	struct utsname buffer;
	if (uname(&buffer) == 0)
	{
		log_print(LOG_INFO, "System: %s %s (%s)", buffer.sysname, buffer.release, buffer.machine);
	}

	pthread_t th;
	pthread_create(&th, NULL, network_thread, NULL);

	app.needs_redraw = 1;
	app.current_state = STATE_HOME;

	service_refresh_contacts();
	service_refresh_requests();
	service_req_conversations();

	int selection = 0;
	int group_selection_idx = 0;
	int group_members_selected[100] = {0};
	char group_name_buf[32] = "New Group";
	char group_desc_buf[64] = "Description";

	while (1)
	{
		// 1. RENDER
		if (app.needs_redraw)
		{
			// UI drawing functions handle NULL pointers gracefully by checking count=0
			if (app.current_state == STATE_HOME)
				ui_draw_home_conversations(app.conversations, app.conv_count, selection);
			else if (app.current_state == STATE_SETTINGS)
				ui_draw_settings(&app.my_info);
			else if (app.current_state == STATE_FRIENDS)
				ui_draw_friends_list(app.contacts, app.contacts_count, selection, app.requests_count);
			else if (app.current_state == STATE_CHAT)
				ui_draw_chat(app.current_conv_name, app.chat_history ? app.chat_history : "", app.chat_input_buffer);
			else if (app.current_state == STATE_REQUESTS)
				ui_draw_requests(app.requests, app.requests_count, selection);
			else if (app.current_state == STATE_CREATE_GROUP)
				ui_draw_create_group_form(group_name_buf, group_desc_buf, app.contacts, app.contacts_count, group_members_selected, group_selection_idx);
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
				{
					app.current_state = STATE_HOME;
					app.needs_redraw = 1;
				}
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

		// 2. INPUT
		int ch = getch();
		if (ch == ERR)
			continue;

		app.needs_redraw = 1;
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
				service_refresh_contacts();
				service_refresh_requests();
			}
			if (ch == 10 || ch == 13)
			{
				if (app.conv_count > 0 && app.conversations)
				{ // Check NULL
					app.current_conv_id = app.conversations[selection].conv_id;
					strncpy(app.current_conv_name, app.conversations[selection].name, 32);

					pthread_mutex_lock(&app.state_lock);
					if (app.chat_history)
					{
						free(app.chat_history);
						app.chat_history = NULL;
					}
					app.chat_history_len = 0;
					pthread_mutex_unlock(&app.state_lock);

					app.conversations[selection].unread_count = 0;
					app.current_state = STATE_CHAT;
					service_req_history(app.current_conv_id);
				}
			}
			if (ch == 's' || ch == 'S')
				app.current_state = STATE_SETTINGS;
		}
		else if (app.current_state == STATE_CHAT)
		{
			if (ch == 27)
			{
				app.current_state = STATE_HOME;
				app.chat_input_buffer[0] = '\0';
				service_req_conversations();
			}
			else if (ch == 10 || ch == 13)
			{
				if (strlen(app.chat_input_buffer) > 0)
				{
					service_send_text(app.current_conv_id, app.chat_input_buffer);
					char line[1200];
					snprintf(line, sizeof(line), "Me: %s\n", app.chat_input_buffer);
					pthread_mutex_lock(&app.state_lock);
					app_append_history(line); // Helper
					pthread_mutex_unlock(&app.state_lock);
					app.chat_input_buffer[0] = '\0';
				}
			}
			else if (ch == KEY_F(1))
			{
				for (int i = 0; i < app.conv_count; i++)
				{
					if (app.conversations[i].conv_id == app.current_conv_id)
					{
						if (app.conversations[i].type == 1)
							app.current_state = STATE_GROUP_SETTINGS;
						break;
					}
				}
			}
			else
			{
				if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
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
				group_members_selected[group_selection_idx] = !group_members_selected[group_selection_idx];
			if (ch == 'n' || ch == 'N')
				ui_input_string(3, 16, "Group Name: ", group_name_buf, 31);
			if (ch == 'd' || ch == 'D')
				ui_input_string(4, 17, "Description: ", group_desc_buf, 63);
			if (ch == 10 || ch == 13)
			{
				uint32_t selected_uids[MAX_PARTICIPANTS];
				int count = 0;
				for (int i = 0; i < app.contacts_count; i++)
				{
					if (group_members_selected[i])
						selected_uids[count++] = app.contacts[i].uid;
				}
				if (service_create_group(group_name_buf, group_desc_buf, selected_uids, count))
					app.current_state = STATE_HOME;
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
			if (ch == 'a' || ch == 'A')
			{
				char code[16];
				ui_prompt_friend_code(code);
				service_add_contact(code);
			}
			if (ch == 'r' || ch == 'R')
			{
				app.current_state = STATE_REQUESTS;
				selection = 0;
			}
			if (ch == 10 || ch == 13)
			{
				if (app.contacts_count > 0 && app.contacts)
				{
					service_create_private(app.contacts[selection].uid, app.contacts[selection].username);
					app.current_state = STATE_HOME;
				}
			}
		}
		else if (app.current_state == STATE_SETTINGS)
		{
			ui_draw_settings(&app.my_info);
			if (ch == 'l' || ch == 'L')
			{
				service_logout();
				break;
			}
			if (ch == 'e' || ch == 'E')
			{
				char u[32] = {0}, p[64] = {0};
				pthread_mutex_lock(&app.state_lock);
				ui_input_string(10, 4, "New Username: ", u, 31);
				ui_input_string(12, 4, "New Password: ", p, 63);
				if (strlen(u) > 0 || strlen(p) > 0)
					service_update_user(u, p);
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
				if (app.requests_count > 0 && app.requests)
				{
					service_decide_request(app.requests[selection].uid, (ch == 10));
					app_remove_request(selection);
				}
			}
		}
		else if (app.current_state == STATE_GROUP_SETTINGS)
		{
			int is_admin = 0;
			for (int i = 0; i < app.conv_count; i++)
				if (app.conversations[i].conv_id == app.current_conv_id)
					is_admin = (app.conversations[i].my_role == 1);

			if (ch == 27 || ch == KEY_BACKSPACE)
				app.current_state = STATE_CHAT;
			if (is_admin)
			{
				if (ch == KEY_DC || ch == 330)
				{
					service_delete_group(app.current_conv_id);
					app.current_state = STATE_HOME;
				}
				if (ch == 'm' || ch == 'M')
				{
					app.current_state = STATE_MANAGE_MEMBERS;
					app.member_selection_idx = 0;
					service_req_members(app.current_conv_id);
				}
			}
			else
			{
				if (ch == 'm' || ch == 'M')
				{
					app.current_state = STATE_MANAGE_MEMBERS;
					service_req_members(app.current_conv_id);
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
			if (ch == 'k' || ch == 'K')
			{
				int is_admin = 0;
				for (int i = 0; i < app.conv_count; i++)
					if (app.conversations[i].conv_id == app.current_conv_id)
						is_admin = (app.conversations[i].my_role == 1);

				if (is_admin && app.current_group_members_count > 0 && app.current_group_members)
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

	ui_cleanup();
	app_cleanup(); // CLEAN MEMORY
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