#ifndef UI_H
#define UI_H
#include <stddef.h>
#include "protocol.h" 

typedef enum UIState {
    STATE_START,
    STATE_REGISTER,
    STATE_LOGIN,
    STATE_HOME,            // Shows Conversations List
		STATE_CREATE_GROUP,
    STATE_CHAT,            // Active Conversation
    STATE_SETTINGS,
		STATE_FRIENDS,
    STATE_REQUESTS,
		STATE_GROUP_SETTINGS,
		STATE_MANAGE_MEMBERS
} UIState;

void ui_init(void);
void ui_cleanup(void);

// Screens
int ui_draw_start_menu(void);
void ui_draw_register(char *email, char *user, char *pass);
void ui_draw_login(char *email, char *pass, int *remember);

// Home shows Conversations
void ui_draw_home_conversations(ConversationSummary *convs, int count, int selection_idx);

void ui_draw_friends_list(ContactSummary *contacts, int count, int selection_idx, int requests_count);
void ui_draw_requests(ContactSummary *requests, int count, int selection_idx);

void ui_draw_chat(const char *conv_name, const char *history, const char *current_input);
void ui_draw_create_group_form(char *name, char *desc, ContactSummary *contacts, int count, int *selected_indices, int selection_idx);
void ui_draw_settings(const MyInfoPayload *info);

void ui_draw_group_settings(ConversationSummary *conv);
void ui_draw_group_members(GroupMemberSummary *members, int count, int selection_idx, int is_admin);

// Helpers
void ui_prompt_friend_code(char *code_out);
void ui_input_string(int y, int x, const char* label, char *buffer, int max_len);
void ui_log_msg(const char *fmt, ...);

#endif