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
#include <openssl/ssl.h>
#include <openssl/err.h>

// System
#include "system/protocol.h"
#include "system/storage.h"
#include "system/logger.h"
#include "system/config_loader.h"

// Infrastructure
#include "infrastructure/server_types.h"
#include "infrastructure/client_manager.h"

// Handlers
#include "handlers/auth_handler.h"
#include "handlers/chat_handler.h"
#include "handlers/group_handler.h"
#include "handlers/contact_handler.h"

#define MAX_EVENTS 64

// Global SSL Context
SSL_CTX *ctx;

int main()
{
	signal(SIGPIPE, SIG_IGN);
	log_init("server");
	// 1. Load Config
	AppConfig config;
	if (config_load("server.conf", &config))
	{
		log_print(LOG_INFO, "Loaded configuration from server.conf");
	}
	else
	{
		log_print(LOG_INFO, "No server.conf found, using defaults (Port: %d)", config.port);
	}

	// 2. Initialize OpenSSL
	init_openssl();
	ctx = create_context(1); // 1 = Server

	configure_context(ctx, config.server_cert_path, config.server_key_path);
	log_print(LOG_INFO, "SSL Context initialized. loaded certs.");

	// 3. DB Init
	init_clients();
	storage_backup(config.db_path);

	if (!storage_init(config.db_path))
	{
		log_print(LOG_ERROR, config.db_path);
		return 1;
	}

	// 4. Socket Bind
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(config.port);

	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	bind(server_fd, (struct sockaddr *)&address, sizeof(address));
	listen(server_fd, 10);

	int epoll_fd = epoll_create1(0);
	struct epoll_event ev, events[MAX_EVENTS];
	ev.events = EPOLLIN;
	ev.data.fd = server_fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

	printf("Secure Server Running on %d...\n", config.port);

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
					// 2. Wrap new socket in SSL
					SSL *ssl = SSL_new(ctx);
					SSL_set_fd(ssl, new_sock);

					// 3. Perform Handshake (Simple blocking for now)
					if (SSL_accept(ssl) <= 0)
					{
						ERR_print_errors_fp(stderr);
						log_print(LOG_ERROR, "SSL Handshake failed for FD %d", new_sock);
						SSL_free(ssl);
						close(new_sock);
					}
					else
					{
						// Handshake success, add to EPoll and Client Manager
						struct timeval tv = {2, 0};
						setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

						ev.events = EPOLLIN;
						ev.data.fd = new_sock;
						epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_sock, &ev);

						add_client(new_sock, ssl);

						// IMPORTANT: Store the SSL pointer in the client struct
						Client *c = get_client_by_fd(new_sock);
						if (c)
							c->ssl = ssl;

						log_print(LOG_INFO, "New secure connection (FD: %d)", new_sock);
					}
				}
			}
			else
			{
				int fd = events[i].data.fd;
				Client *cli = get_client_by_fd(fd);

				// Safety check
				if (!cli || !cli->ssl)
				{
					// Should not happen if logic is correct
					close(fd);
					remove_client(fd);
					continue;
				}

				MessageType type;
				void *payload = NULL;
				uint32_t len;

				// 4. Decrypt Packet using SSL pointer
				if (recv_packet(cli->ssl, &type, &payload, &len) <= 0)
				{
					if (cli->uid > 0)
						log_print(LOG_INFO, "User %s disconnected", cli->username);

					// Cleanup handled by remove_client (SSL_free, SSL_shutdown)
					close(fd);
					remove_client(fd);
					continue;
				}

				// --- DISPATCHER ---
				switch (type)
				{
				// Auth
				case MSG_REGISTER:
					handle_register(cli, (RegisterPayload *)payload);
					break;
				case MSG_LOGIN:
					handle_login(cli, (LoginPayload *)payload);
					break;
				case MSG_UPDATE_USER:
					handle_update_user(cli, (UpdateUserPayload *)payload);
					break;

				// Chat
				case MSG_SEND_TEXT:
					handle_send_text(cli, (SendMessagePayload *)payload);
					break;
				case MSG_REQ_HISTORY:
					handle_req_history(cli, (RequestHistoryPayload *)payload);
					break;

				// Groups / Conversations
				case MSG_CREATE_CONV:
					handle_create_conv(cli, (CreateConvPayload *)payload);
					break;
				case MSG_REQ_CONVERSATIONS:
					handle_req_conversations(cli);
					break;
				case MSG_UPDATE_GROUP:
					handle_update_group(cli, (UpdateGroupPayload *)payload);
					break;
				case MSG_ADD_MEMBER:
					handle_add_member(cli, (AddMemberPayload *)payload);
					break;
				case MSG_REQ_MEMBERS:
					handle_req_members(cli, (ReqMembersPayload *)payload);
					break;
				case MSG_KICK_MEMBER:
					handle_kick_member(cli, (KickMemberPayload *)payload);
					break;
				case MSG_DELETE_GROUP:
					handle_delete_group(cli, (DeleteGroupPayload *)payload);
					break;

				// Contacts
				case MSG_REQ_CONTACTS:
					handle_req_contacts(cli);
					break;
				case MSG_ADD_BY_CODE:
					handle_add_by_code(cli, (AddContactPayload *)payload);
					break;
				case MSG_GET_REQUESTS:
					handle_get_requests(cli);
					break;
				case MSG_DECIDE_REQUEST:
					handle_decide_request(cli, (DecideRequestPayload *)payload);
					break;

				default:
					log_print(LOG_WARN, "Unknown packet type %d from FD %d", type, fd);
					break;
				}

				if (payload)
					free(payload);
			}
		}
	}

	// Cleanup
	cleanup_openssl();
	storage_close();
	log_close();
	return 0;
}