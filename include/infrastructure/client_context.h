/**
 * @file client_context.h
 * @brief Defines the Client's global state and memory management.
 */

#ifndef CLIENT_CONTEXT_H
#define CLIENT_CONTEXT_H

#include "system/protocol.h"
#include "infrastructure/ui.h"
#include <pthread.h>
#include <openssl/ssl.h>

/**
 * @brief Global Application State.
 * Holds the connection handle, UI state flags, and dynamic data cache.
 */
typedef struct AppState
{
	int sock_fd;	/**< Raw socket file descriptor */
	SSL *ssl;			/**< OpenSSL Connection Handle */
	SSL_CTX *ctx; /**< OpenSSL Global Context */

	UIState current_state; /**< Current screen being displayed */
	int needs_redraw;			 /**< Flag: 1 if UI needs refreshing */

	// Identity
	MyInfoPayload my_info;

	// --- Dynamic Data Stores ---
	ContactSummary *contacts;
	int contacts_count;

	ContactSummary *requests;
	int requests_count;

	ConversationSummary *conversations;
	int conv_count;

	GroupMemberSummary *current_group_members;
	int current_group_members_count;
	int member_selection_idx;

	// --- Active Chat State ---
	uint32_t current_conv_id;
	char current_conv_name[32];

	char *chat_history; /**< Dynamic string buffer for chat history */
	size_t chat_history_len;

	char chat_input_buffer[MAX_TEXT_LEN];

	// Notification State
	int pending_notify;
	char notify_title[64];
	char notify_msg[128];

	// Thread Safety
	pthread_mutex_t state_lock; /**< Protects access to all fields above */
} AppState;

extern AppState app;

/**
 * @brief Initializes the app state, mutexes, and sets pointers to NULL.
 */
void app_init(void);

/**
 * @brief Frees all dynamic memory in AppState and destroys the mutex.
 */
void app_cleanup(void);

// Logic Helpers
void app_remove_request(int index);
void app_append_history(const char *text);

// Session Persistence
void save_session(const char *e, const char *p);
int load_session(LoginPayload *p);
void clear_session();

#endif