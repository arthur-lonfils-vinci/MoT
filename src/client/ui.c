#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include "infrastructure/ui.h"

static WINDOW *win_main;
static pthread_mutex_t ui_lock = PTHREAD_MUTEX_INITIALIZER;
static int width, height;

void ui_init()
{
	initscr();
	cbreak();
	noecho();
	curs_set(0);
	timeout(100);
	keypad(stdscr, TRUE);
	if (has_colors())
	{
		start_color();
		init_pair(1, COLOR_WHITE, COLOR_BLUE);
		init_pair(2, COLOR_GREEN, COLOR_BLACK);
		init_pair(3, COLOR_RED, COLOR_BLACK);
	}
	getmaxyx(stdscr, height, width);
	win_main = newwin(height, width, 0, 0);
}

void ui_cleanup()
{
	delwin(win_main);
	endwin();
}

int ui_draw_start_menu()
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);
	mvwprintw(win_main, height / 3, width / 2 - 10, "=== MoT v0.5 ===");
	mvwprintw(win_main, height / 3 + 2, width / 2 - 8, "[1] Login");
	mvwprintw(win_main, height / 3 + 3, width / 2 - 8, "[2] Register");
	mvwprintw(win_main, height / 3 + 4, width / 2 - 8, "[3] Quit");
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
	while (1)
	{
		int ch = wgetch(win_main);
		if (ch == '1')
			return 1;
		if (ch == '2')
			return 2;
		if (ch == '3' || ch == 'q')
			return 3;
	}
}

void ui_draw_register(char *email, char *user, char *pass)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);
	wattron(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 2, 2, " REGISTER ");
	wattroff(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 5, 4, "Email: ");
	mvwprintw(win_main, 7, 4, "Username: ");
	mvwprintw(win_main, 9, 4, "Password: ");
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
	echo();
	curs_set(1);
	mvwgetnstr(win_main, 5, 12, email, MAX_EMAIL_LEN - 1);
	mvwgetnstr(win_main, 7, 15, user, MAX_NAME_LEN - 1);
	mvwgetnstr(win_main, 9, 15, pass, MAX_PASS_LEN - 1);
	noecho();
	curs_set(0);
}

void ui_draw_login(char *email, char *pass, int *remember)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);
	wattron(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 2, 2, " LOGIN ");
	wattroff(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 5, 4, "Email: ");
	mvwprintw(win_main, 7, 4, "Password: ");
	mvwprintw(win_main, 9, 4, "Remember me? (y/n): ");
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
	echo();
	curs_set(1);
	mvwgetnstr(win_main, 5, 12, email, MAX_EMAIL_LEN - 1);
	mvwgetnstr(win_main, 7, 15, pass, MAX_PASS_LEN - 1);

	int ch = mvwgetch(win_main, 9, 24);
	if (ch == 'y' || ch == 'Y')
	{
		*remember = 1;
		waddstr(win_main, " Yes");
	}
	else
	{
		*remember = 0;
		waddstr(win_main, " No");
	}
	wrefresh(win_main);
	napms(200);

	noecho();
	curs_set(0);
}

void ui_draw_home_conversations(ConversationSummary *convs, int count, int selection_idx)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);

	wattron(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 1, 2, " CONVERSATIONS ");
	wattroff(win_main, COLOR_PAIR(1));

	mvwprintw(win_main, height - 2, 2, "[F] Friends  [G] New Group  [S] Settings  [Q] Quit");
	if (count == 0)
	{
		mvwprintw(win_main, 3, 4, "(No active conversations.)");
	}

	for (int i = 0; i < count; i++)
	{
		char label[128];
		char type_indicator = (convs[i].type == CONV_TYPE_GROUP) ? '#' : '@';

		if (convs[i].unread_count > 0)
			snprintf(label, 128, "%c %s (%d)", type_indicator, convs[i].name, convs[i].unread_count);
		else
			snprintf(label, 128, "%c %s", type_indicator, convs[i].name);

		if (i == selection_idx)
		{
			wattron(win_main, COLOR_PAIR(2));
			mvwprintw(win_main, 3 + i, 4, "> %s ", label);
			wattroff(win_main, COLOR_PAIR(2));
		}
		else
		{
			if (convs[i].unread_count > 0)
				wattron(win_main, COLOR_PAIR(3));
			mvwprintw(win_main, 3 + i, 4, "  %s ", label);
			if (convs[i].unread_count > 0)
				wattroff(win_main, COLOR_PAIR(3));
		}
	}
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_create_group_form(char *name, char *desc, ContactSummary *contacts, int count, int *selected_indices, int selection_idx)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);

	wattron(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 1, 2, " CREATE GROUP ");
	wattroff(win_main, COLOR_PAIR(1));

	mvwprintw(win_main, 3, 4, "Group Name: %s", name);
	mvwprintw(win_main, 4, 4, "Description: %s", desc);
	mvwprintw(win_main, 6, 4, "Select Members (Space to toggle, Enter to Create):");

	for (int i = 0; i < count; i++)
	{
		const char *marker = selected_indices[i] ? "[x]" : "[ ]";
		if (i == selection_idx)
		{
			wattron(win_main, COLOR_PAIR(2));
			mvwprintw(win_main, 8 + i, 6, "> %s %s", marker, contacts[i].username);
			wattroff(win_main, COLOR_PAIR(2));
		}
		else
		{
			mvwprintw(win_main, 8 + i, 6, "  %s %s", marker, contacts[i].username);
		}
	}

	mvwprintw(win_main, height - 2, 2, "[N] Edit Name  [D] Edit Desc  [Space] Select  [Enter] Create  [Esc] Cancel");
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_friends_list(ContactSummary *contacts, int count, int selection_idx, int requests_count) {
    pthread_mutex_lock(&ui_lock);
    wclear(win_main);
    box(win_main, 0, 0);

    wattron(win_main, COLOR_PAIR(1));
    mvwprintw(win_main, 1, 2, " FRIENDS ");
    wattroff(win_main, COLOR_PAIR(1));

    // Footer with commands
    if (requests_count > 0) {
        wattron(win_main, COLOR_PAIR(3)); // Highlight requests if pending
        mvwprintw(win_main, height - 2, 20, "[R] Requests (%d)", requests_count);
        wattroff(win_main, COLOR_PAIR(3));
        mvwprintw(win_main, height - 2, 2, "[A] Add Friend");
        mvwprintw(win_main, height - 2, 38, "[Enter] Chat  [Esc] Back");
    } else {
        mvwprintw(win_main, height - 2, 2, "[A] Add Friend  [R] Requests  [Enter] Chat  [Esc] Back");
    }

    if (count == 0) {
        mvwprintw(win_main, 3, 4, "(No friends yet. Press [A] to add one!)");
    }

    for (int i = 0; i < count; i++) {
        if (i == selection_idx) {
            wattron(win_main, COLOR_PAIR(2));
            mvwprintw(win_main, 3 + i, 4, "> %s ", contacts[i].username);
            wattroff(win_main, COLOR_PAIR(2));
        } else {
            mvwprintw(win_main, 3 + i, 4, "  %s ", contacts[i].username);
        }
    }
    wrefresh(win_main);
    pthread_mutex_unlock(&ui_lock);
}

void ui_draw_chat(const char *conv_name, const char *history, const char *current_input)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);

	char title[64];
	snprintf(title, 64, " %s ", conv_name);

	const char *help_text = "[F1] Settings  [ESC] Back";
	int help_len = strlen(help_text);
	int max_title_len = width - help_len - 5;

	if ((int)strlen(title) > max_title_len)
	{
		title[max_title_len] = '\0';
		title[max_title_len - 1] = '.';
		title[max_title_len - 2] = '.';
		title[max_title_len - 3] = '.';
	}

	wattron(win_main, COLOR_PAIR(1) | A_BOLD);
	mvwprintw(win_main, 1, 2, "%s", title);
	wattroff(win_main, COLOR_PAIR(1) | A_BOLD);

	mvwprintw(win_main, 1, width - help_len - 2, "%s", help_text);

	char temp_hist[16384];
	strncpy(temp_hist, history, 16384);
	temp_hist[16383] = '\0';

	char *line = strtok(temp_hist, "\n");
	int row = 3;
	while (line && row < height - 3)
	{
		if (strncmp(line, "Me:", 3) == 0)
		{
			wattron(win_main, COLOR_PAIR(2));
			mvwprintw(win_main, row, 2, "%s", line);
			wattroff(win_main, COLOR_PAIR(2));
		}
		else
		{
			mvwprintw(win_main, row, 2, "%s", line);
		}
		line = strtok(NULL, "\n");
		row++;
	}

	mvwhline(win_main, height - 3, 1, ACS_HLINE, width - 2);
	mvwprintw(win_main, height - 2, 2, "> %s_", current_input);

	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_settings(const MyInfoPayload *info)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);
	wattron(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 1, 2, " SETTINGS ");
	wattroff(win_main, COLOR_PAIR(1));

	mvwprintw(win_main, 3, 4, "ID:           %d", info->uid);
	mvwprintw(win_main, 4, 4, "Username:     %s", info->username);
	mvwprintw(win_main, 5, 4, "Email:        %s", info->email);
	mvwprintw(win_main, 7, 4, "FRIEND CODE:  %s", info->friend_code);
	mvwprintw(win_main, height - 4, 4, "[E] Edit Profile   [L] Logout");
	mvwprintw(win_main, height - 2, 4, "[Backspace] Return");
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_requests(ContactSummary *requests, int count, int selection_idx)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);
	wattron(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 1, 2, " REQUESTS ");
	wattroff(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, height - 2, 2, "[Enter] Accept  [Del] Deny  [Back] Return");

	if (count == 0)
		mvwprintw(win_main, 3, 4, "(No pending requests)");
	for (int i = 0; i < count; i++)
	{
		if (i == selection_idx)
		{
			wattron(win_main, COLOR_PAIR(2));
			mvwprintw(win_main, 3 + i, 4, "> %s ", requests[i].username);
			wattroff(win_main, COLOR_PAIR(2));
		}
		else
		{
			mvwprintw(win_main, 3 + i, 4, "  %s ", requests[i].username);
		}
	}
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
}

void ui_draw_group_members(GroupMemberSummary *members, int count, int selection_idx, int is_admin)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);
	wattron(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 1, 2, " MANAGE MEMBERS ");
	wattroff(win_main, COLOR_PAIR(1));

	if (count == 0)
		mvwprintw(win_main, 3, 4, "(Loading...)");

	for (int i = 0; i < count; i++)
	{
		char label[64];
		snprintf(label, 64, "%s %s", members[i].username, (members[i].role == 1) ? "[ADMIN]" : "");

		if (i == selection_idx)
		{
			wattron(win_main, COLOR_PAIR(2));
			mvwprintw(win_main, 3 + i, 4, "> %s ", label);
			wattroff(win_main, COLOR_PAIR(2));
		}
		else
		{
			mvwprintw(win_main, 3 + i, 4, "  %s ", label);
		}
	}

	if (is_admin)
	{
		mvwprintw(win_main, height - 2, 2, "[K] Kick Selected  [Esc] Back");
	}
	else
	{
		mvwprintw(win_main, height - 2, 2, "[Esc] Back");
	}

	wrefresh(win_main);
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

void ui_draw_group_settings(ConversationSummary *conv)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_main);
	box(win_main, 0, 0);
	wattron(win_main, COLOR_PAIR(1));
	mvwprintw(win_main, 1, 2, " GROUP SETTINGS ");
	wattroff(win_main, COLOR_PAIR(1));

	mvwprintw(win_main, 3, 4, "Name:        %s", conv->name);
	mvwprintw(win_main, 4, 4, "Description: %s", conv->description);
	mvwprintw(win_main, 6, 4, "My Role:     %s", (conv->my_role == 1) ? "ADMIN" : "MEMBER");

	if (conv->my_role == 1)
	{ // Admin
		mvwprintw(win_main, 8, 4, "[N] Edit Name");
		mvwprintw(win_main, 9, 4, "[D] Edit Description");
		mvwprintw(win_main, 10, 4, "[A] Add Member (Friend Code)");
		mvwprintw(win_main, 11, 4, "[M] Manage/Kick Members");
		wattron(win_main, COLOR_PAIR(3)); // Red text
		mvwprintw(win_main, 13, 4, "[Del] DELETE GROUP");
		wattroff(win_main, COLOR_PAIR(3));
	}
	else
	{
		mvwprintw(win_main, 8, 4, "[M] View Members");
	}

	mvwprintw(win_main, height - 2, 4, "[Esc] Back");
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);
}

void ui_input_string(int y, int x, const char *label, char *buffer, int max_len)
{
	pthread_mutex_lock(&ui_lock);
	mvwprintw(win_main, y, x, "%s", label);
	// Clear the rest of the line for input
	for (int i = 0; i < max_len; i++)
		waddch(win_main, '_');
	wrefresh(win_main);
	pthread_mutex_unlock(&ui_lock);

	echo();
	curs_set(1);
	wmove(win_main, y, x + strlen(label));
	wgetnstr(win_main, buffer, max_len);
	noecho();
	curs_set(0);
}