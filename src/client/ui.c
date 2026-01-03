#define _POSIX_C_SOURCE 200809L
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <libnotify/notify.h>
#include "infrastructure/ui.h"

// --- Global UI State ---
static WINDOW *win_sidebar = NULL;
static WINDOW *win_chat = NULL;
static WINDOW *win_input = NULL;
static WINDOW *win_full = NULL; // For overlays/login

static pthread_mutex_t ui_lock = PTHREAD_MUTEX_INITIALIZER;
static int width, height;
static int sidebar_width;

// Notification initialized flag
static int notify_ready = 0;

// --- Helper: Destroy Windows ---
static void destroy_windows()
{
	if (win_sidebar)
		delwin(win_sidebar);
	if (win_chat)
		delwin(win_chat);
	if (win_input)
		delwin(win_input);
	if (win_full)
		delwin(win_full);
	win_sidebar = win_chat = win_input = win_full = NULL;
}

// --- NEW: Resize Logic ---
void ui_resize()
{
	pthread_mutex_lock(&ui_lock);

	// 1. Destroy old windows
	destroy_windows();

	// 2. Update ncurses dimensions
	endwin();
	refresh();
	getmaxyx(stdscr, height, width);

	// 3. Recalculate Layout
	sidebar_width = width / 4;
	if (sidebar_width < UI_SIDEBAR_MIN_WIDTH)
		sidebar_width = UI_SIDEBAR_MIN_WIDTH;
	if (sidebar_width > 40)
		sidebar_width = 40;

	// --- LAYOUT DEFINITION ---
	// Sidebar: Full Height (Left)
	win_sidebar = newwin(height, sidebar_width, 0, 0);

	// Chat: Top-Right (Height - Input)
	win_chat = newwin(height - UI_INPUT_HEIGHT, width - sidebar_width, 0, sidebar_width);

	// Input: Bottom-Right (Aligned with Chat)
	win_input = newwin(UI_INPUT_HEIGHT, width - sidebar_width, height - UI_INPUT_HEIGHT, sidebar_width);

	// Full: For Overlays
	win_full = newwin(height, width, 0, 0);

	// 4. Redraw standard boxes immediately to prevent artifacts
	box(win_full, 0, 0);
	wrefresh(win_full);

	pthread_mutex_unlock(&ui_lock);
}

// --- Initialization & Cleanup ---

void ui_init()
{
	initscr();
	cbreak();
	noecho();
	curs_set(1); // Visible cursor for typing
	timeout(50); // 50ms timeout for responsive input loop
	keypad(stdscr, TRUE);

	// Modern Terminal Features
	if (has_colors())
	{
		start_color();
		use_default_colors();
		init_pair(1, COLOR_WHITE, COLOR_BLUE);	// Header
		init_pair(2, COLOR_GREEN, -1);					// User/Me
		init_pair(3, COLOR_CYAN, -1);						// System
		init_pair(4, COLOR_YELLOW, -1);					// Alerts
		init_pair(5, COLOR_BLACK, COLOR_WHITE); // Selection
		init_pair(6, COLOR_WHITE, COLOR_RED);		// Danger
	}

	// Enable Mouse
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	printf("\033[?1002h"); // Enable X11 mouse tracking

	// Initial Window Creation
	ui_resize();

	if (notify_init("MoT Client"))
	{
		notify_ready = 1;
	}
}

void ui_cleanup()
{
	if (notify_ready)
		notify_uninit();
	if (win_sidebar)
		delwin(win_sidebar);
	if (win_chat)
		delwin(win_chat);
	if (win_input)
		delwin(win_input);
	if (win_full)
		delwin(win_full);
	endwin();
	printf("\033[?1002l"); // Disable mouse tracking
}

void ui_show_notification(const char *title, const char *msg)
{
	if (!notify_ready)
		return;

	NotifyNotification *n = notify_notification_new(title, msg, "dialog-information");
	notify_notification_set_timeout(n, 4000); // 4 seconds
	notify_notification_show(n, NULL);
	g_object_unref(G_OBJECT(n));
}

// --- Dashboard Logic ---

void ui_refresh_dashboard(ConversationSummary *convs, int count, int selected_idx,
													const char *chat_history, const char *input_buf)
{
	pthread_mutex_lock(&ui_lock);

	// --- 1. SIDEBAR ---
	wclear(win_sidebar);
	// Draw Separation Line
	mvwvline(win_sidebar, 0, sidebar_width - 1, ACS_VLINE, height);

	// Header
	wattron(win_sidebar, A_BOLD | COLOR_PAIR(1));
	mvwhline(win_sidebar, 0, 0, ' ', sidebar_width - 1);
	mvwprintw(win_sidebar, 0, 1, "MESSAGES (%d)", count);
	wattroff(win_sidebar, A_BOLD | COLOR_PAIR(1));

	// List
	int list_height = height - 5; // Reserve space for bottom buttons
	for (int i = 0; i < count; i++)
	{
		int row = 2 + i;
		if (row >= list_height)
			break;

		char label[64];
		char icon = (convs[i].type == 1) ? '#' : '@';
		snprintf(label, sidebar_width - 3, "%c %s", icon, convs[i].name);

		if (i == selected_idx)
		{
			wattron(win_sidebar, COLOR_PAIR(5));
			mvwprintw(win_sidebar, row, 1, "%-*s", sidebar_width - 3, label);
			wattroff(win_sidebar, COLOR_PAIR(5));
		}
		else
		{
			mvwprintw(win_sidebar, row, 1, "%s", label);
		}

		if (convs[i].unread_count > 0)
		{
			wattron(win_sidebar, COLOR_PAIR(4) | A_BOLD);
			mvwprintw(win_sidebar, row, sidebar_width - 5, "(%d)", convs[i].unread_count);
			wattroff(win_sidebar, COLOR_PAIR(4) | A_BOLD);
		}
	}

	// --- NEW: Bottom Buttons ---
	mvwhline(win_sidebar, height - 4, 0, ACS_HLINE, sidebar_width - 1);

	// Button 1: F1 Friends
	wattron(win_sidebar, A_BOLD);
	mvwprintw(win_sidebar, height - 3, 1, "[F1] Friends");

	// Button 2: F2 Settings
	mvwprintw(win_sidebar, height - 2, 1, "[F2] Settings");

	// Button 3: F3 New Group
	mvwprintw(win_sidebar, height - 1, 1, "[F3] New Group");
	wattroff(win_sidebar, A_BOLD);

	wrefresh(win_sidebar);

	// --- 2. CHAT WINDOW ---
	wclear(win_chat);

	// Header
	if (selected_idx >= 0 && selected_idx < count)
	{
		wattron(win_chat, A_BOLD | COLOR_PAIR(1));
		mvwhline(win_chat, 0, 0, ' ', width - sidebar_width);
		mvwprintw(win_chat, 0, 1, " %s ", convs[selected_idx].name);

		char help[64] = "[F1] Friends  [F2] Settings";
		mvwprintw(win_chat, 0, width - sidebar_width - strlen(help) - 2, "%s", help);
		wattroff(win_chat, A_BOLD | COLOR_PAIR(1));
	}

	// Content
	if (chat_history && strlen(chat_history) > 0)
	{
		wmove(win_chat, 2, 1);
		wprintw(win_chat, "%s", chat_history);
	}
	else
	{
		wattron(win_chat, A_DIM);
		mvwprintw(win_chat, (height / 2) - 2, 5, "No messages yet...");
		wattroff(win_chat, A_DIM);
	}
	wrefresh(win_chat);

	// --- 3. INPUT AREA ---
	wclear(win_input);
	mvwhline(win_input, 0, 0, ACS_HLINE, width - sidebar_width);
	mvwprintw(win_input, 1, 1, "> %s", input_buf);
	wmove(win_input, 1, 3 + strlen(input_buf));
	wrefresh(win_input);

	pthread_mutex_unlock(&ui_lock);
}

// Returns:
// 1=ConvClick, 2=ScrollUp, 3=ScrollDown,
// 4=F1(Friends), 5=F2(Settings), 6=F3(NewGroup)
int ui_handle_mouse_dashboard(int *clicked_conv_idx)
{
	MEVENT event;
	if (getmouse(&event) == OK)
	{
		// --- SIDEBAR CLICKS ---
		if (event.x < sidebar_width)
		{
			// Check Bottom Buttons
			if (event.y == height - 3)
				return 4; // F1 Friends
			if (event.y == height - 2)
				return 5; // F2 Settings
			if (event.y == height - 1)
				return 6; // F3 Group

			// Check Conversation List
			int row = event.y;
			int list_start_y = 2;
			int index = row - list_start_y;

			// Ensure we don't click below the list (into the button area)
			if (index >= 0 && row < height - 4)
			{
				*clicked_conv_idx = index;
				return 1;
			}
		}

		// --- SCROLL WHEEL ---
		if (event.bstate & BUTTON4_PRESSED)
			return 2; // Up
		if (event.bstate & BUTTON5_PRESSED)
			return 3; // Down
	}
	return 0;
}

// --- Overlay Implementations ---

void ui_input_string(int y, int x, const char *label, char *buffer, int max_len)
{
	pthread_mutex_lock(&ui_lock);
	mvwprintw(win_full, y, x, "%s", label);
	for (int i = 0; i < max_len; i++)
		waddch(win_full, ' ');
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);

	echo();
	curs_set(1);
	wmove(win_full, y, x + strlen(label));
	wgetnstr(win_full, buffer, max_len);
	noecho();
	curs_set(0);
}

int ui_draw_start_menu()
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);
	mvwprintw(win_full, height / 3, width / 2 - 14, "=== Messenger of Things ===");
	mvwprintw(win_full, height / 3 + 2, width / 2 - 8, "[1] Login");
	mvwprintw(win_full, height / 3 + 3, width / 2 - 8, "[2] Register");
	mvwprintw(win_full, height / 3 + 4, width / 2 - 8, "[3] Quit");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
	while (1)
	{
		int ch = wgetch(win_full);
		if (ch == '1')
			return 1;
		if (ch == '2')
			return 2;
		if (ch == '3' || ch == 'q')
			return 3;
	}
}

void ui_draw_first_start_wizard(char *host_buffer, int host_len, int *port_out)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);

	// Title
	wattron(win_full, A_BOLD);
	mvwprintw(win_full, height / 2 - 6, (width - 20) / 2, "WELCOME TO MoT");
	wattroff(win_full, A_BOLD);

	// Instructions
	mvwprintw(win_full, height / 2 - 3, (width - 60) / 2, "This looks like your first time running the client.");
	mvwprintw(win_full, height / 2 - 2, (width - 60) / 2, "Please configure the server connection details (or leave blank).");

	// Warning (Red if supported)
	if (has_colors())
		wattron(win_full, COLOR_PAIR(3));
	mvwprintw(win_full, height / 2 - 1, (width - 60) / 2, "(!) Warning:");
	mvwprintw(win_full, height / 2 + 0, (width - 60) / 2, "Changing these values implies a CA certificate change.");
	mvwprintw(win_full, height / 2 + 1, (width - 60) / 2, "If using a custom server, ensure you have the correct certificates.");
	if (has_colors())
		wattroff(win_full, COLOR_PAIR(3));

	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);

	// Defined Defaults
	const char *def_host = "server-mot.arthur-server.com";
	const char *def_port = "8010";
	char port_str[8] = {0};

	// Note: We display the default in the prompt so the user knows
	char prompt_host[128];
	snprintf(prompt_host, sizeof(prompt_host), "Server Host [%s]: ", def_host);

	ui_input_string(height / 2 + 3, (width - 70) / 2, prompt_host, host_buffer, host_len - 1);

	if (strlen(host_buffer) == 0)
	{
		strncpy(host_buffer, def_host, host_len - 1);
		// Visual feedback that default was applied
		pthread_mutex_lock(&ui_lock);
		mvwprintw(win_full, height / 2 + 3, (width - 70) / 2 + strlen(prompt_host), "%s", def_host);
		pthread_mutex_unlock(&ui_lock);
	}

	char prompt_port[64];
	snprintf(prompt_port, sizeof(prompt_port), "Server Port [%s]: ", def_port);

	ui_input_string(height / 2 + 5, (width - 70) / 2, prompt_port, port_str, 6);

	// CHECK: If empty, apply default
	if (strlen(port_str) == 0)
	{
		strncpy(port_str, def_port, 7);
		// Visual feedback
		pthread_mutex_lock(&ui_lock);
		mvwprintw(win_full, height / 2 + 5, (width - 70) / 2 + strlen(prompt_port), "%s", def_port);
		pthread_mutex_unlock(&ui_lock);
	}

	*port_out = atoi(port_str);
	if (*port_out <= 0 || *port_out > 65535)
		*port_out = 8010;

	pthread_mutex_lock(&ui_lock);
	mvwprintw(win_full, height / 2 + 8, (width - 40) / 2, "Configuration saved! Press any key...");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);

	wgetch(win_full);
}

void ui_draw_register(char *email, char *user, char *pass)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);
	wattron(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 2, 2, " REGISTER ");
	wattroff(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 5, 4, "Email: ");
	mvwprintw(win_full, 7, 4, "Username: ");
	mvwprintw(win_full, 9, 4, "Password: ");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
	echo();
	curs_set(1);
	mvwgetnstr(win_full, 5, 12, email, MAX_EMAIL_LEN - 1);
	mvwgetnstr(win_full, 7, 15, user, MAX_NAME_LEN - 1);
	mvwgetnstr(win_full, 9, 15, pass, MAX_PASS_LEN - 1);
	noecho();
	curs_set(0);
}

void ui_draw_login(char *email, char *pass, int *remember)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);
	wattron(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 2, 2, " LOGIN ");
	wattroff(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 5, 4, "Email: ");
	mvwprintw(win_full, 7, 4, "Password: ");
	mvwprintw(win_full, 9, 4, "Remember me? (y/n): ");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
	echo();
	curs_set(1);
	mvwgetnstr(win_full, 5, 12, email, MAX_EMAIL_LEN - 1);
	mvwgetnstr(win_full, 7, 15, pass, MAX_PASS_LEN - 1);

	int ch = mvwgetch(win_full, 9, 24);
	if (ch == 'y' || ch == 'Y')
	{
		*remember = 1;
		waddstr(win_full, " Yes");
	}
	else
	{
		*remember = 0;
		waddstr(win_full, " No");
	}
	wrefresh(win_full);
	napms(200);

	noecho();
	curs_set(0);
}

void ui_draw_settings(const MyInfoPayload *info)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);
	wattron(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 1, 2, " SETTINGS ");
	wattroff(win_full, COLOR_PAIR(1));

	mvwprintw(win_full, 3, 4, "ID:           %d", info->uid);
	mvwprintw(win_full, 4, 4, "Username:     %s", info->username);
	mvwprintw(win_full, 5, 4, "Email:        %s", info->email);
	mvwprintw(win_full, 7, 4, "FRIEND CODE:  %s", info->friend_code);
	mvwprintw(win_full, height - 4, 4, "[E] Edit Profile   [L] Logout");
	mvwprintw(win_full, height - 2, 4, "[Backspace] Return");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_friends_list(ContactSummary *contacts, int count, int selection_idx, int requests_count)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);

	wattron(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 1, 2, " FRIENDS ");
	wattroff(win_full, COLOR_PAIR(1));

	// Footer with commands
	if (requests_count > 0)
	{
		wattron(win_full, COLOR_PAIR(3)); // Highlight requests if pending
		mvwprintw(win_full, height - 2, 20, "[R] Requests (%d)", requests_count);
		wattroff(win_full, COLOR_PAIR(3));
		mvwprintw(win_full, height - 2, 2, "[A] Add Friend");
		mvwprintw(win_full, height - 2, 38, "[Enter] Chat  [Esc] Back");
	}
	else
	{
		mvwprintw(win_full, height - 2, 2, "[A] Add Friend  [R] Requests  [Enter] Chat  [Esc] Back");
	}

	if (count == 0)
	{
		mvwprintw(win_full, 3, 4, "(No friends yet. Press [A] to add one!)");
	}

	for (int i = 0; i < count; i++)
	{
		if (i == selection_idx)
		{
			wattron(win_full, COLOR_PAIR(2));
			mvwprintw(win_full, 3 + i, 4, "> %s ", contacts[i].username);
			wattroff(win_full, COLOR_PAIR(2));
		}
		else
		{
			mvwprintw(win_full, 3 + i, 4, "  %s ", contacts[i].username);
		}
	}
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_requests(ContactSummary *requests, int count, int selection_idx)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);
	wattron(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 1, 2, " REQUESTS ");
	wattroff(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, height - 2, 2, "[Enter] Accept  [Del] Deny  [Back] Return");

	if (count == 0)
		mvwprintw(win_full, 3, 4, "(No pending requests)");
	for (int i = 0; i < count; i++)
	{
		if (i == selection_idx)
		{
			wattron(win_full, COLOR_PAIR(2));
			mvwprintw(win_full, 3 + i, 4, "> %s ", requests[i].username);
			wattroff(win_full, COLOR_PAIR(2));
		}
		else
		{
			mvwprintw(win_full, 3 + i, 4, "  %s ", requests[i].username);
		}
	}
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_create_group_form(char *name, char *desc, ContactSummary *contacts, int count, int *selected_indices, int selection_idx)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);

	wattron(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 1, 2, " CREATE GROUP ");
	wattroff(win_full, COLOR_PAIR(1));

	mvwprintw(win_full, 3, 4, "Group Name: %s", name);
	mvwprintw(win_full, 4, 4, "Description: %s", desc);
	mvwprintw(win_full, 6, 4, "Select Members (Space to toggle, Enter to Create):");

	for (int i = 0; i < count; i++)
	{
		const char *marker = selected_indices[i] ? "[x]" : "[ ]";
		if (i == selection_idx)
		{
			wattron(win_full, COLOR_PAIR(2));
			mvwprintw(win_full, 8 + i, 6, "> %s %s", marker, contacts[i].username);
			wattroff(win_full, COLOR_PAIR(2));
		}
		else
		{
			mvwprintw(win_full, 8 + i, 6, "  %s %s", marker, contacts[i].username);
		}
	}

	mvwprintw(win_full, height - 2, 2, "[N] Edit Name  [D] Edit Desc  [Space] Select  [Enter] Create  [Esc] Cancel");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_group_settings(ConversationSummary *conv)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);
	wattron(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 1, 2, " GROUP SETTINGS ");
	wattroff(win_full, COLOR_PAIR(1));

	mvwprintw(win_full, 3, 4, "Name:        %s", conv->name);
	mvwprintw(win_full, 4, 4, "Description: %s", conv->description);
	mvwprintw(win_full, 6, 4, "My Role:     %s", (conv->my_role == 1) ? "ADMIN" : "MEMBER");

	if (conv->my_role == 1)
	{ // Admin
		mvwprintw(win_full, 8, 4, "[N] Edit Name");
		mvwprintw(win_full, 9, 4, "[D] Edit Description");
		mvwprintw(win_full, 10, 4, "[A] Add Member (Friend Code)");
		mvwprintw(win_full, 11, 4, "[M] Manage/Kick Members");
		wattron(win_full, COLOR_PAIR(3)); // Red text
		mvwprintw(win_full, 13, 4, "[Del] DELETE GROUP");
		wattroff(win_full, COLOR_PAIR(3));
	}
	else
	{
		mvwprintw(win_full, 8, 4, "[M] View Members");
	}

	mvwprintw(win_full, height - 2, 4, "[Esc] Back");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_group_members(GroupMemberSummary *members, int count, int selection_idx, int is_admin)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_full);
	box(win_full, 0, 0);
	wattron(win_full, COLOR_PAIR(1));
	mvwprintw(win_full, 1, 2, " MANAGE MEMBERS ");
	wattroff(win_full, COLOR_PAIR(1));

	if (count == 0)
		mvwprintw(win_full, 3, 4, "(Loading...)");

	for (int i = 0; i < count; i++)
	{
		char label[64];
		snprintf(label, 64, "%s %s", members[i].username, (members[i].role == 1) ? "[ADMIN]" : "");

		if (i == selection_idx)
		{
			wattron(win_full, COLOR_PAIR(2));
			mvwprintw(win_full, 3 + i, 4, "> %s ", label);
			wattroff(win_full, COLOR_PAIR(2));
		}
		else
		{
			mvwprintw(win_full, 3 + i, 4, "  %s ", label);
		}
	}

	if (is_admin)
	{
		mvwprintw(win_full, height - 2, 2, "[K] Kick Selected  [Esc] Back");
	}
	else
	{
		mvwprintw(win_full, height - 2, 2, "[Esc] Back");
	}

	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
}

void ui_prompt_friend_code(char *code_out)
{
	WINDOW *popup = newwin(5, 40, height / 2 - 2, width / 2 - 20);
	box(popup, 0, 0);
	mvwprintw(popup, 1, 2, "Enter Friend Code (6 chars):");
	wrefresh(popup);
	echo();
	curs_set(1);
	mvwgetnstr(popup, 2, 2, code_out, 10);
	noecho();
	curs_set(0);
	delwin(popup);
}