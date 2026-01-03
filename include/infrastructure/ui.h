/**
 * @file ui.h
 * @brief Modern Text User Interface (TUI) definitions using Ncurses.
 */

#ifndef UI_H
#define UI_H

#include <stddef.h>
#include "system/protocol.h"

// --- Layout Constants ---
#define UI_INPUT_HEIGHT 3
#define UI_SIDEBAR_MIN_WIDTH 20

typedef enum UIState
{
	STATE_START,
	STATE_REGISTER,
	STATE_LOGIN,
	STATE_DASHBOARD,			/**< Unified View: Sidebar + Chat + Input */
	STATE_SETTINGS,				/**< Overlay */
	STATE_FRIENDS,				/**< Overlay */
	STATE_REQUESTS,				/**< Overlay */
	STATE_CREATE_GROUP,		/**< Overlay */
	STATE_GROUP_SETTINGS, /**< Overlay */
	STATE_MANAGE_MEMBERS	/**< Overlay */
} UIState;

/**
 * @brief Starts Ncurses mode, mouse support, and libnotify.
 */
void ui_init(void);

/**
 * @brief Ends Ncurses mode and cleans up notifications.
 */
void ui_cleanup(void);

/**
 * @brief Shows a desktop notification (Thread-safe wrapper recommended).
 */
void ui_show_notification(const char *title, const char *msg);

// --- Dashboard Drawing ---

/**
 * @brief Renders the complete split-screen dashboard.
 * @param convs Array of conversations for the sidebar.
 * @param count Number of conversations.
 * @param selected_idx Index of the currently active conversation in the sidebar.
 * @param chat_history Full history buffer of the selected conversation.
 * @param input_buf Current text in the input field.
 */
void ui_refresh_dashboard(ConversationSummary *convs, int count, int selected_idx,
													const char *chat_history, const char *input_buf);

/**
 * @brief Handles mouse events for the dashboard.
 * @param clicked_conv_idx Output pointer: set to the index of the conversation clicked in the sidebar.
 * @return 1 if a conversation was clicked, 2 if scrolled, 0 otherwise.
 */
int ui_handle_mouse_dashboard(int *clicked_conv_idx);

// --- Overlay/Startup Functions (Keep these for modal interactions) ---
void ui_draw_first_start_wizard(char *host_buffer, int host_len, int *port_out);
int ui_draw_start_menu(void);
void ui_draw_register(char *email, char *user, char *pass);
void ui_draw_login(char *email, char *pass, int *remember);

// Helper Overlays
void ui_draw_friends_list(ContactSummary *contacts, int count, int selection_idx, int requests_count);
void ui_draw_requests(ContactSummary *requests, int count, int selection_idx);
void ui_draw_create_group_form(char *name, char *desc, ContactSummary *contacts, int count, int *selected_indices, int selection_idx);
void ui_draw_settings(const MyInfoPayload *info);
void ui_draw_group_settings(ConversationSummary *conv);
void ui_draw_group_members(GroupMemberSummary *members, int count, int selection_idx, int is_admin);

// --- Input Helpers ---
void ui_prompt_friend_code(char *code_out);
void ui_input_string(int y, int x, const char *label, char *buffer, int max_len);

#endif