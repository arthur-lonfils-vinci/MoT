#ifndef CLIENT_CONTEXT_H
#define CLIENT_CONTEXT_H

#include "protocol.h"
#include "ui.h"
#include <pthread.h>

// Holds the entire state of the application
typedef struct AppState {
    int sock_fd;
    UIState current_state;
		int needs_redraw;
    
    // Identity
    MyInfoPayload my_info; 
    
    // --- Data Stores ---
    ContactSummary contacts[100];
    int contacts_count;

    ContactSummary requests[100];
    int requests_count;
    
    // Conversations
    ConversationSummary conversations[100];
    int conv_count;

		// Store members of the current group (for Admin view)
		GroupMemberSummary current_group_members[MAX_MEMBERS];
    int current_group_members_count;
    int member_selection_idx;
    
    // Active Chat State
    uint32_t current_conv_id;
    char current_conv_name[32];
    char chat_history[16384];
    char chat_input_buffer[MAX_TEXT_LEN];

    // Thread Safety
    pthread_mutex_t state_lock;
} AppState;

extern AppState app;

// Lifecycle
void app_init(void);

// Logic Helpers
void app_remove_request(int index);

// Session
void save_session(const char *e, const char *p);
int load_session(LoginPayload *p);
void clear_session();

#endif