#ifndef UI_H
#define UI_H

#include <stddef.h>
#include "system/protocol.h"

#define UI_INPUT_HEIGHT 3
#define UI_SIDEBAR_MIN_WIDTH 20

typedef enum UIState
{
	STATE_START,
	STATE_REGISTER,
	STATE_LOGIN,
	STATE_DASHBOARD,
	STATE_SETTINGS,
	STATE_FRIENDS,
	STATE_REQUESTS,
	STATE_CREATE_GROUP,
	STATE_GROUP_SETTINGS,
	STATE_MANAGE_MEMBERS
} UIState;

void ui_init(void);
void ui_cleanup(void);
void ui_show_notification(const char *title, const char *msg);
void ui_resize(void);

// Dashboard
void ui_refresh_dashboard(ConversationSummary *convs, int count, int selected_idx,
													const char *chat_history, const char *input_buf);
int ui_handle_mouse_dashboard(int *clicked_conv_idx);

// Menus & Modals
int ui_draw_start_menu(void);
void ui_draw_first_start_wizard(char *host, int len, int *port);

// Auth Forms (Return 1 = Submit, 0 = Back)
int ui_draw_login(char *email, char *pass, int *remember);
int ui_draw_register(char *email, char *user, char *pass);

// Overlays
void ui_draw_settings(const MyInfoPayload *info);
int ui_draw_friends_list(ContactSummary *contacts, int count, int *selection_idx, int requests_count);
int ui_draw_requests(ContactSummary *requests, int count, int *selection_idx);

// Group Forms
int ui_draw_create_group_form(char *name, char *desc, ContactSummary *contacts, int count, int *selected_indices, int *selection_idx);
int ui_draw_group_settings(ConversationSummary *conv);
int ui_draw_group_members(GroupMemberSummary *members, int count, int *selection_idx, int is_admin);

// Prompts
void ui_prompt_friend_code(char *code_out);
void ui_prompt_input(const char *title, const char *label, char *buffer, int max_len);

#endif