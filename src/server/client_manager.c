#include "infrastructure/client_manager.h"
#include <openssl/ssl.h>
#include <string.h>
#include <stdio.h>

// Internal Linked List Node
typedef struct ClientNode
{
	Client data;
	struct ClientNode *next;
} ClientNode;

static ClientNode *head = NULL;

void init_clients(void)
{
	head = NULL;
}

void free_clients(void)
{
	ClientNode *current = head;
	while (current != NULL)
	{
		ClientNode *next = current->next;
		// Cleanup SSL if it wasn't already
		if (current->data.ssl)
		{
			SSL_shutdown(current->data.ssl);
			SSL_free(current->data.ssl);
		}
		free(current);
		current = next;
	}
	head = NULL;
}

void add_client(int fd, SSL *ssl)
{
	ClientNode *node = malloc(sizeof(ClientNode));
	if (!node)
		return;

	memset(&node->data, 0, sizeof(Client));
	node->data.fd = fd;
	node->data.ssl = ssl;
	node->data.uid = 0;
	node->data.is_online = 0;

	node->next = head;
	head = node;
}

void remove_client(int fd)
{
	ClientNode *current = head;
	ClientNode *prev = NULL;

	while (current != NULL)
	{
		if (current->data.fd == fd)
		{
			if (prev == NULL)
			{
				head = current->next;
			}
			else
			{
				prev->next = current->next;
			}

			if (current->data.ssl)
			{
				SSL_shutdown(current->data.ssl);
				SSL_free(current->data.ssl);
			}
			free(current);
			return;
		}
		prev = current;
		current = current->next;
	}
}

Client *get_client_by_uid(uint32_t uid)
{
	ClientNode *current = head;
	while (current != NULL)
	{
		if (current->data.uid == uid && current->data.is_online)
		{
			return &current->data;
		}
		current = current->next;
	}
	return NULL;
}

Client *get_client_by_fd(int fd)
{
	ClientNode *current = head;
	while (current != NULL)
	{
		if (current->data.fd == fd)
		{
			return &current->data;
		}
		current = current->next;
	}
	return NULL;
}