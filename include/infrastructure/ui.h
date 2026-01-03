/**
 * @file ui.h
 * @brief Text User Interface (TUI) definitions using Ncurses.
 */

#ifndef UI_H
#define UI_H
#include <stddef.h>
#include "system/protocol.h" 

typedef enum UIState {
    STATE_START,
    STATE_REGISTER,
    STATE_LOGIN,
    STATE_HOME,            /**< Conversation list view */
    STATE_CREATE_GROUP,
    STATE_CHAT,            /**< Active messaging view */
    STATE_SETTINGS,
    STATE_FRIENDS,
    STATE_REQUESTS,
    STATE_GROUP_SETTINGS,
    STATE_MANAGE_MEMBERS
} UIState;

/**
 * @brief Starts Ncurses mode and initializes colors/windows.
 */
void ui_init(void);

/**
 * @brief Ends Ncurses mode.
 */
void ui_cleanup(void);

// --- Drawing Functions ---
// Each function draws a specific screen based on the provided data.
/**
 * @brief Draws the First Start Wizard to configure Host/Port.
 * @param host_buffer Output buffer for hostname.
 * @param host_len Size of host_buffer.
 * @param port_out Pointer to integer for port.
 */
void ui_draw_first_start_wizard(char *host_buffer, int host_len, int *port_out);

int ui_draw_start_menu(void);
void ui_draw_register(char *email, char *user, char *pass);
void ui_draw_login(char *email, char *pass, int *remember);

void ui_draw_home_conversations(ConversationSummary *convs, int count, int selection_idx);
void ui_draw_friends_list(ContactSummary *contacts, int count, int selection_idx, int requests_count);
void ui_draw_requests(ContactSummary *requests, int count, int selection_idx);
void ui_draw_chat(const char *conv_name, const char *history, const char *current_input);
void ui_draw_create_group_form(char *name, char *desc, ContactSummary *contacts, int count, int *selected_indices, int selection_idx);
void ui_draw_settings(const MyInfoPayload *info);
void ui_draw_group_settings(ConversationSummary *conv);
void ui_draw_group_members(GroupMemberSummary *members, int count, int selection_idx, int is_admin);

// --- Helpers ---
void ui_prompt_friend_code(char *code_out);
void ui_input_string(int y, int x, const char* label, char *buffer, int max_len);

#endif