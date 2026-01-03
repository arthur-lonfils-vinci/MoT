#define _POSIX_C_SOURCE 200809L
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <libnotify/notify.h>
#include "infrastructure/ui.h"
#include "infrastructure/theme.h"

static WINDOW *win_sidebar = NULL;
static WINDOW *win_chat = NULL;
static WINDOW *win_input = NULL;
static WINDOW *win_full = NULL;

static pthread_mutex_t ui_lock = PTHREAD_MUTEX_INITIALIZER;
static int width, height;
static int sidebar_width;
static int notify_ready = 0;

static int current_theme_idx = 0;
static const char *theme_names[] = {"Classic Blue", "Dark Mode", "Hacker Green"};

// --- Theme ---
void ui_theme_init(int saved_index)
{
	if (!has_colors())
		return;
	start_color();
	use_default_colors();
	if (saved_index < 0 || saved_index > 2)
		saved_index = 0;
	current_theme_idx = saved_index - 1; // Cycle will increment it
	ui_theme_cycle();
}

int ui_theme_get_index() { return current_theme_idx; }

void ui_theme_cycle()
{
	if (!has_colors())
		return;
	current_theme_idx = (current_theme_idx + 1) % 3;
	switch (current_theme_idx)
	{
	case 0: // Classic
		init_pair(THEME_PAIR_NORMAL, COLOR_WHITE, -1);
		init_pair(THEME_PAIR_HEADER, COLOR_WHITE, COLOR_BLUE);
		init_pair(THEME_PAIR_HIGHLIGHT, COLOR_BLACK, COLOR_CYAN);
		init_pair(THEME_PAIR_USER, COLOR_GREEN, -1);
		init_pair(THEME_PAIR_SYSTEM, COLOR_YELLOW, -1);
		init_pair(THEME_PAIR_ALERT, COLOR_WHITE, COLOR_RED);
		init_pair(THEME_PAIR_DANGER, COLOR_RED, -1);
		init_pair(THEME_PAIR_INPUT, COLOR_WHITE, COLOR_BLACK);
		break;
	case 1: // Dark
		init_pair(THEME_PAIR_NORMAL, COLOR_WHITE, -1);
		init_pair(THEME_PAIR_HEADER, COLOR_BLACK, COLOR_WHITE);
		init_pair(THEME_PAIR_HIGHLIGHT, COLOR_BLACK, COLOR_YELLOW);
		init_pair(THEME_PAIR_USER, COLOR_CYAN, -1);
		init_pair(THEME_PAIR_SYSTEM, COLOR_MAGENTA, -1);
		init_pair(THEME_PAIR_ALERT, COLOR_RED, -1);
		init_pair(THEME_PAIR_DANGER, COLOR_RED, -1);
		init_pair(THEME_PAIR_INPUT, COLOR_WHITE, COLOR_BLACK);
		break;
	case 2: // Hacker
		init_pair(THEME_PAIR_NORMAL, COLOR_GREEN, COLOR_BLACK);
		init_pair(THEME_PAIR_HEADER, COLOR_BLACK, COLOR_GREEN);
		init_pair(THEME_PAIR_HIGHLIGHT, COLOR_BLACK, COLOR_GREEN);
		init_pair(THEME_PAIR_USER, COLOR_GREEN, -1);
		init_pair(THEME_PAIR_SYSTEM, COLOR_GREEN, -1);
		init_pair(THEME_PAIR_ALERT, COLOR_GREEN, -1);
		init_pair(THEME_PAIR_DANGER, COLOR_GREEN, -1);
		init_pair(THEME_PAIR_INPUT, COLOR_GREEN, COLOR_BLACK);
		break;
	}
	if (win_sidebar)
		wbkgd(win_sidebar, COLOR_PAIR(THEME_PAIR_NORMAL));
	if (win_chat)
		wbkgd(win_chat, COLOR_PAIR(THEME_PAIR_NORMAL));
	if (win_input)
		wbkgd(win_input, COLOR_PAIR(THEME_PAIR_NORMAL));
	if (win_full)
		wbkgd(win_full, COLOR_PAIR(THEME_PAIR_NORMAL));
}

const char *ui_theme_get_name() { return theme_names[current_theme_idx]; }

// --- Window Mgmt ---
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

void ui_resize()
{
	pthread_mutex_lock(&ui_lock);
	destroy_windows();
	endwin();
	refresh();
	getmaxyx(stdscr, height, width);
	sidebar_width = width / 4;
	if (sidebar_width < 25)
		sidebar_width = 25;
	if (sidebar_width > 40)
		sidebar_width = 40;
	win_sidebar = newwin(height, sidebar_width, 0, 0);
	win_chat = newwin(height - UI_INPUT_HEIGHT, width - sidebar_width, 0, sidebar_width);
	win_input = newwin(UI_INPUT_HEIGHT, width - sidebar_width, height - UI_INPUT_HEIGHT, sidebar_width);
	win_full = newwin(height, width, 0, 0);
	keypad(win_full, TRUE);
	pthread_mutex_unlock(&ui_lock);
}

void ui_init()
{
	initscr();
	cbreak();
	noecho();
	curs_set(1);
	timeout(50);
	keypad(stdscr, TRUE);
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	printf("\033[?1002h");
	ui_theme_init(0); // Default, updated later by main
	ui_resize();
	if (notify_init("MoT Client"))
		notify_ready = 1;
}

void ui_cleanup()
{
	if (notify_ready)
		notify_uninit();
	destroy_windows();
	endwin();
	printf("\033[?1002l");
}

void ui_show_notification(const char *title, const char *msg)
{
	if (!notify_ready)
		return;
	NotifyNotification *n = notify_notification_new(title, msg, "dialog-information");
	notify_notification_set_timeout(n, 4000);
	notify_notification_show(n, NULL);
	g_object_unref(G_OBJECT(n));
}

// --- Helper: Draw Centered Box ---
static void draw_modal_box(const char *title, int *y_out, int *x_out, int *w_out, int *h_out)
{
	wclear(win_full);
	int box_w = 60;
	int box_h = height - 8;
	if (box_w > width - 4)
		box_w = width - 4;
	if (box_h < 12)
		box_h = 12;
	int start_y = (height - box_h) / 2;
	int start_x = (width - box_w) / 2;

	for (int i = start_y; i < start_y + box_h; i++)
		mvwhline(win_full, i, start_x, ' ', box_w);
	wattron(win_full, A_BOLD);
	mvwaddch(win_full, start_y, start_x, ACS_ULCORNER);
	mvwhline(win_full, start_y, start_x + 1, ACS_HLINE, box_w - 2);
	mvwaddch(win_full, start_y, start_x + box_w - 1, ACS_URCORNER);
	mvwvline(win_full, start_y + 1, start_x, ACS_VLINE, box_h - 2);
	mvwvline(win_full, start_y + 1, start_x + box_w - 1, ACS_VLINE, box_h - 2);
	mvwaddch(win_full, start_y + box_h - 1, start_x, ACS_LLCORNER);
	mvwhline(win_full, start_y + box_h - 1, start_x + 1, ACS_HLINE, box_w - 2);
	mvwaddch(win_full, start_y + box_h - 1, start_x + box_w - 1, ACS_LRCORNER);
	wattroff(win_full, A_BOLD);

	wattron(win_full, COLOR_PAIR(THEME_PAIR_HEADER) | A_BOLD);
	mvwhline(win_full, start_y, start_x + 1, ' ', box_w - 2);
	mvwprintw(win_full, start_y, start_x + (box_w - strlen(title)) / 2, "%s", title);
	wattroff(win_full, COLOR_PAIR(THEME_PAIR_HEADER) | A_BOLD);
	*y_out = start_y;
	*x_out = start_x;
	*w_out = box_w;
	*h_out = box_h;
}

// --- Dashboard ---
void ui_refresh_dashboard(ConversationSummary *convs, int count, int selected_idx, const char *chat_history, const char *input_buf)
{
	pthread_mutex_lock(&ui_lock);
	wclear(win_sidebar);
	wattron(win_sidebar, COLOR_PAIR(THEME_PAIR_HEADER) | A_BOLD);
	mvwhline(win_sidebar, 0, 0, ' ', sidebar_width);
	mvwprintw(win_sidebar, 0, 1, " MoT MESSENGER");
	wattroff(win_sidebar, COLOR_PAIR(THEME_PAIR_HEADER) | A_BOLD);
	mvwvline(win_sidebar, 0, sidebar_width - 1, ACS_VLINE, height);

	int list_h = height - 6; // Adjusted for Quit button
	for (int i = 0; i < count; i++)
	{
		if (i >= list_h)
			break;
		char icon = (convs[i].type == 1) ? '#' : '@';
		char buf[64];
		snprintf(buf, sidebar_width - 2, "%c %s", icon, convs[i].name);
		if (i == selected_idx)
		{
			wattron(win_sidebar, COLOR_PAIR(THEME_PAIR_HIGHLIGHT));
			mvwprintw(win_sidebar, i + 2, 1, "%-*s", sidebar_width - 3, buf);
			wattroff(win_sidebar, COLOR_PAIR(THEME_PAIR_HIGHLIGHT));
		}
		else
			mvwprintw(win_sidebar, i + 2, 1, "%s", buf);
		if (convs[i].unread_count > 0)
		{
			wattron(win_sidebar, COLOR_PAIR(THEME_PAIR_ALERT) | A_BOLD);
			mvwprintw(win_sidebar, i + 2, sidebar_width - 5, "(%d)", convs[i].unread_count);
			wattroff(win_sidebar, COLOR_PAIR(THEME_PAIR_ALERT) | A_BOLD);
		}
	}
	int fy = height - 5;
	mvwhline(win_sidebar, fy, 0, ACS_HLINE, sidebar_width - 1);
	wattron(win_sidebar, A_DIM);
	mvwprintw(win_sidebar, fy + 1, 1, "[F1] Friends");
	mvwprintw(win_sidebar, fy + 2, 1, "[F2] Settings");
	mvwprintw(win_sidebar, fy + 3, 1, "[F3] New Group");

	// Quit Button
	wattroff(win_sidebar, A_DIM);
	wattron(win_sidebar, COLOR_PAIR(THEME_PAIR_DANGER));
	mvwprintw(win_sidebar, fy + 4, 1, "[ESC]  Quit App");
	wattroff(win_sidebar, COLOR_PAIR(THEME_PAIR_DANGER));

	wrefresh(win_sidebar);

	wclear(win_chat);
	if (count > 0 && selected_idx < count)
	{
		wattron(win_chat, COLOR_PAIR(THEME_PAIR_HEADER) | A_BOLD);
		mvwhline(win_chat, 0, 0, ' ', width - sidebar_width);
		mvwprintw(win_chat, 0, 1, " %s ", convs[selected_idx].name);
		if (convs[selected_idx].type == 1)
		{
			char *btn = "[F4] Details";
			mvwprintw(win_chat, 0, width - sidebar_width - strlen(btn) - 2, "%s", btn);
		}
		wattroff(win_chat, COLOR_PAIR(THEME_PAIR_HEADER) | A_BOLD);
		if (chat_history)
			mvwprintw(win_chat, 2, 1, "%s", chat_history);
	}
	else
		mvwprintw(win_chat, height / 2, (width - sidebar_width) / 2 - 8, "Select a chat");
	wrefresh(win_chat);

	wclear(win_input);
	mvwhline(win_input, 0, 0, ACS_HLINE, width - sidebar_width);
	mvwprintw(win_input, 1, 1, "> %s", input_buf);
	wmove(win_input, 1, 3 + strlen(input_buf));
	wrefresh(win_input);
	pthread_mutex_unlock(&ui_lock);
}

int ui_handle_mouse_dashboard(int *idx)
{
	MEVENT e;
	if (getmouse(&e) != OK)
		return 0;
	if (e.x < sidebar_width)
	{
		if (e.y == height - 4)
			return 4;
		if (e.y == height - 3)
			return 5;
		if (e.y == height - 2)
			return 6;
		if (e.y == height - 1)
			return 9; // Q
		if (e.y >= 2 && e.y < height - 5)
		{
			*idx = e.y - 2;
			return 1;
		}
	}
	else if (e.y == 0 && e.x > width - 20)
		return 7;
	return 0;
}

// --- Prompt ---
void ui_prompt_input(const char *title, const char *label, char *buffer, int max_len)
{
	pthread_mutex_lock(&ui_lock);
	int by, bx, bw, bh;
	draw_modal_box(title, &by, &bx, &bw, &bh);
	mvwprintw(win_full, by + 3, bx + 2, "%s", label);
	wattron(win_full, COLOR_PAIR(THEME_PAIR_INPUT));
	for (int k = 0; k < max_len; k++)
		mvwaddch(win_full, by + 3, bx + 2 + strlen(label) + 1 + k, '_');
	wattroff(win_full, COLOR_PAIR(THEME_PAIR_INPUT));
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);

	echo();
	curs_set(1);
	mvwgetnstr(win_full, by + 3, bx + 2 + strlen(label) + 1, buffer, max_len);
	noecho();
	curs_set(0);
}

// --- Modals (NON-BLOCKING) ---

// Returns: 0=Stay, 1=Back, 2=Action...
void ui_draw_settings(const MyInfoPayload *info)
{
	pthread_mutex_lock(&ui_lock);
	int by, bx, bw, bh;
	draw_modal_box("USER SETTINGS", &by, &bx, &bw, &bh);
	mvwprintw(win_full, by + 3, bx + 4, "ID:   %d", info->uid);
	mvwprintw(win_full, by + 4, bx + 4, "User: %s", info->username);
	mvwprintw(win_full, by + 5, bx + 4, "Mail: %s", info->email);
	wattron(win_full, COLOR_PAIR(THEME_PAIR_HIGHLIGHT));
	mvwprintw(win_full, by + 7, bx + 4, " Code: %s ", info->friend_code);
	wattroff(win_full, COLOR_PAIR(THEME_PAIR_HIGHLIGHT));

	mvwprintw(win_full, by + 10, bx + 4, "Theme:");
	wattron(win_full, A_REVERSE);
	mvwprintw(win_full, by + 10, bx + 12, "[ %s ]", ui_theme_get_name());
	wattroff(win_full, A_REVERSE);
	mvwprintw(win_full, by + 10, bx + 32, "(Press T)");

	mvwprintw(win_full, by + 12, bx + 4, "[E] Edit Profile");
	mvwprintw(win_full, by + 12, bx + 25, "[L] Logout");
	mvwprintw(win_full, by + 14, bx + 4, "[ESC] Back");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
}

int ui_draw_friends_list(ContactSummary *c, int cnt, int *idx, int r)
{
	pthread_mutex_lock(&ui_lock);
	int by, bx, bw, bh;
	draw_modal_box("FRIENDS", &by, &bx, &bw, &bh);
	int list_h = bh - 6;
	if (cnt == 0)
		mvwprintw(win_full, by + 3, bx + 4, "(No friends)");
	for (int i = 0; i < cnt; i++)
	{
		if (i >= list_h)
			break;
		if (i == *idx)
			wattron(win_full, COLOR_PAIR(THEME_PAIR_HIGHLIGHT));
		mvwprintw(win_full, by + 3 + i, bx + 4, " %-30s [CHAT] ", c[i].username);
		if (i == *idx)
			wattroff(win_full, COLOR_PAIR(THEME_PAIR_HIGHLIGHT));
	}
	mvwprintw(win_full, by + bh - 2, bx + 2, "[A] Add  [R] Req(%d)  [ESC] Back", r);
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
	return 0; // Input handled in main
}

int ui_draw_requests(ContactSummary *req, int cnt, int *idx)
{
	pthread_mutex_lock(&ui_lock);
	int by, bx, bw, bh;
	draw_modal_box("REQUESTS", &by, &bx, &bw, &bh);
	int list_h = bh - 6;
	if (cnt == 0)
		mvwprintw(win_full, by + 3, bx + 4, "(No requests)");
	for (int i = 0; i < cnt; i++)
	{
		if (i >= list_h)
			break;
		if (i == *idx)
			wattron(win_full, COLOR_PAIR(THEME_PAIR_HIGHLIGHT));
		mvwprintw(win_full, by + 3 + i, bx + 4, " %-30s ", req[i].username);
		if (i == *idx)
			wattroff(win_full, COLOR_PAIR(THEME_PAIR_HIGHLIGHT));
	}
	mvwprintw(win_full, by + bh - 2, bx + 2, "[Ent] Accept  [Del] Deny  [ESC] Back");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
	return 0;
}

int ui_draw_create_group_form(char *n, char *d, ContactSummary *c, int cnt, int *sel, int *idx)
{
	pthread_mutex_lock(&ui_lock);
	int by, bx, bw, bh;
	draw_modal_box("NEW GROUP", &by, &bx, &bw, &bh);
	mvwprintw(win_full, by + 3, bx + 4, "Name: %s", n);
	mvwprintw(win_full, by + 4, bx + 4, "Desc: %s", d);
	int list_start = by + 6;
	for (int i = 0; i < cnt; i++)
	{
		if (list_start + i >= by + bh - 3)
			break;
		mvwprintw(win_full, list_start + i, bx + 4, (i == *idx) ? "> [%c] %s" : "  [%c] %s", sel[i] ? 'X' : ' ', c[i].username);
	}
	mvwprintw(win_full, by + bh - 2, bx + 2, "[N] Name [D] Desc [Spc] Toggle [Ent] Create");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
	return 0;
}

int ui_draw_group_settings(ConversationSummary *c)
{
	pthread_mutex_lock(&ui_lock);
	int by, bx, bw, bh;
	draw_modal_box("GROUP INFO", &by, &bx, &bw, &bh);
	mvwprintw(win_full, by + 3, bx + 4, "Name: %s", c->name);
	mvwprintw(win_full, by + 4, bx + 4, "Desc: %s", c->description);
	if (c->my_role == 1)
		mvwprintw(win_full, by + 7, bx + 4, "[M] Manage Members  [Del] Delete");
	else
		mvwprintw(win_full, by + 7, bx + 4, "[M] View Members");
	mvwprintw(win_full, by + bh - 2, bx + 4, "[Esc] Back");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
	return 0;
}

int ui_draw_group_members(GroupMemberSummary *m, int cnt, int *idx, int adm)
{
	pthread_mutex_lock(&ui_lock);
	int by, bx, bw, bh;
	draw_modal_box("MEMBERS", &by, &bx, &bw, &bh);
	for (int i = 0; i < cnt; i++)
		mvwprintw(win_full, by + 3 + i, bx + 4, i == *idx ? "> %s" : "  %s", m[i].username);
	if (adm)
		mvwprintw(win_full, by + bh - 2, bx + 2, "[K] Kick Selected  [Esc] Back");
	else
		mvwprintw(win_full, by + bh - 2, bx + 2, "[Esc] Back");
	wrefresh(win_full);
	pthread_mutex_unlock(&ui_lock);
	return 0;
}

// ... Keep blocking forms for auth (safe context) ...
static void draw_field(int y, int x, int w, const char *lbl, const char *val, int sel, int pass)
{
	mvwprintw(win_full, y, x, "%s", lbl);
	int fx = x + strlen(lbl) + 1;
	int fw = w - strlen(lbl) - 1;
	if (sel)
		wattron(win_full, COLOR_PAIR(THEME_PAIR_INPUT) | A_REVERSE);
	else
		wattron(win_full, COLOR_PAIR(THEME_PAIR_INPUT));
	for (int i = 0; i < fw; i++)
		mvwaddch(win_full, y, fx + i, ' ');
	char buf[128];
	if (pass)
	{
		memset(buf, '*', strlen(val));
		buf[strlen(val)] = 0;
	}
	else
		strcpy(buf, val);
	mvwprintw(win_full, y, fx, " %s", buf);
	if (sel)
		wattroff(win_full, COLOR_PAIR(THEME_PAIR_INPUT) | A_REVERSE);
	else
		wattroff(win_full, COLOR_PAIR(THEME_PAIR_INPUT));
}

int ui_draw_login(char *email, char *pass, int *remember)
{
	int sel = 0, run = 1, res = 0;
	while (run)
	{
		pthread_mutex_lock(&ui_lock);
		int by, bx, bw, bh;
		draw_modal_box("LOGIN", &by, &bx, &bw, &bh);
		draw_field(by + 4, bx + 4, bw - 8, "Email:", email, (sel == 0), 0);
		draw_field(by + 6, bx + 4, bw - 8, "Password:", pass, (sel == 1), 1);
		if (sel == 2)
			wattron(win_full, A_REVERSE);
		mvwprintw(win_full, by + 8, bx + 4, "[%c] Remember me", *remember ? 'X' : ' ');
		if (sel == 2)
			wattroff(win_full, A_REVERSE);
		if (sel == 3)
			wattron(win_full, A_REVERSE);
		mvwprintw(win_full, by + 10, bx + bw / 2 - 10, "[ LOGIN ]");
		if (sel == 3)
			wattroff(win_full, A_REVERSE);
		if (sel == 4)
			wattron(win_full, A_REVERSE);
		mvwprintw(win_full, by + 10, bx + bw / 2 + 2, "[ BACK ]");
		if (sel == 4)
			wattroff(win_full, A_REVERSE);
		wrefresh(win_full);
		pthread_mutex_unlock(&ui_lock);

		int ch = wgetch(win_full);
		if (ch == KEY_RESIZE)
		{
			ui_resize();
			continue;
		}
		if (ch == '\t' || ch == KEY_DOWN)
		{
			sel++;
			if (sel > 4)
				sel = 0;
		}
		else if (ch == KEY_BTAB || ch == KEY_UP)
		{
			sel--;
			if (sel < 0)
				sel = 4;
		}
		else if (ch == '\n')
		{
			if (sel == 3)
			{
				res = 1;
				run = 0;
			}
			else if (sel == 4)
			{
				res = 0;
				run = 0;
			}
			else if (sel == 2)
				*remember = !(*remember);
			else
				sel++;
		}
		else if (ch == 27)
		{
			res = 0;
			run = 0;
		}
		else if (ch == KEY_MOUSE)
		{
			MEVENT e;
			if (getmouse(&e) == OK)
			{
				if (e.y == by + 4)
					sel = 0;
				else if (e.y == by + 6)
					sel = 1;
				else if (e.y == by + 8)
				{
					sel = 2;
					*remember = !(*remember);
				}
				else if (e.y == by + 10)
				{
					if (e.x < bx + bw / 2)
					{
						sel = 3;
						res = 1;
						run = 0;
					}
					else
					{
						sel = 4;
						res = 0;
						run = 0;
					}
				}
			}
		}
		else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
		{
			if (sel == 0 && strlen(email) > 0)
				email[strlen(email) - 1] = 0;
			if (sel == 1 && strlen(pass) > 0)
				pass[strlen(pass) - 1] = 0;
		}
		else if (isprint(ch))
		{
			if (sel == 0 && strlen(email) < 63)
			{
				int l = strlen(email);
				email[l] = ch;
				email[l + 1] = 0;
			}
			if (sel == 1 && strlen(pass) < 63)
			{
				int l = strlen(pass);
				pass[l] = ch;
				pass[l + 1] = 0;
			}
		}
	}
	return res;
}

int ui_draw_register(char *email, char *user, char *pass)
{
	int sel = 0, run = 1, res = 0;
	while (run)
	{
		pthread_mutex_lock(&ui_lock);
		int by, bx, bw, bh;
		draw_modal_box("REGISTER", &by, &bx, &bw, &bh);
		draw_field(by + 4, bx + 4, bw - 8, "Email:", email, (sel == 0), 0);
		draw_field(by + 6, bx + 4, bw - 8, "Username:", user, (sel == 1), 0);
		draw_field(by + 8, bx + 4, bw - 8, "Password:", pass, (sel == 2), 1);
		if (sel == 3)
			wattron(win_full, A_REVERSE);
		mvwprintw(win_full, by + 11, bx + bw / 2 - 12, "[ REGISTER ]");
		if (sel == 3)
			wattroff(win_full, A_REVERSE);
		if (sel == 4)
			wattron(win_full, A_REVERSE);
		mvwprintw(win_full, by + 11, bx + bw / 2 + 2, "[ BACK ]");
		if (sel == 4)
			wattroff(win_full, A_REVERSE);
		wrefresh(win_full);
		pthread_mutex_unlock(&ui_lock);

		int ch = wgetch(win_full);
		if (ch == KEY_RESIZE)
		{
			ui_resize();
			continue;
		}
		if (ch == '\t' || ch == KEY_DOWN)
		{
			sel++;
			if (sel > 4)
				sel = 0;
		}
		else if (ch == KEY_BTAB || ch == KEY_UP)
		{
			sel--;
			if (sel < 0)
				sel = 4;
		}
		else if (ch == '\n')
		{
			if (sel == 3)
			{
				res = 1;
				run = 0;
			}
			else if (sel == 4)
			{
				res = 0;
				run = 0;
			}
			else
				sel++;
		}
		else if (ch == 27)
		{
			res = 0;
			run = 0;
		}
		else if (ch == KEY_MOUSE)
		{
			MEVENT e;
			if (getmouse(&e) == OK)
			{
				if (e.y == by + 4)
					sel = 0;
				else if (e.y == by + 6)
					sel = 1;
				else if (e.y == by + 8)
					sel = 2;
				else if (e.y == by + 11)
				{
					if (e.x < bx + bw / 2)
					{
						sel = 3;
						res = 1;
						run = 0;
					}
					else
					{
						sel = 4;
						res = 0;
						run = 0;
					}
				}
			}
		}
		else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8)
		{
			if (sel == 0 && strlen(email) > 0)
				email[strlen(email) - 1] = 0;
			if (sel == 1 && strlen(user) > 0)
				user[strlen(user) - 1] = 0;
			if (sel == 2 && strlen(pass) > 0)
				pass[strlen(pass) - 1] = 0;
		}
		else if (isprint(ch))
		{
			if (sel == 0 && strlen(email) < 63)
			{
				int l = strlen(email);
				email[l] = ch;
				email[l + 1] = 0;
			}
			if (sel == 1 && strlen(user) < 31)
			{
				int l = strlen(user);
				user[l] = ch;
				user[l + 1] = 0;
			}
			if (sel == 2 && strlen(pass) < 63)
			{
				int l = strlen(pass);
				pass[l] = ch;
				pass[l + 1] = 0;
			}
		}
	}
	return res;
}

int ui_draw_start_menu()
{
	while (1)
	{
		pthread_mutex_lock(&ui_lock);
		wclear(win_full);
		box(win_full, 0, 0);
		int cy = height / 2 - 5;
		int cx = width / 2;
		wattron(win_full, A_BOLD | COLOR_PAIR(THEME_PAIR_HEADER));
		mvwprintw(win_full, cy, cx - 10, "   MoT MESSENGER   ");
		wattroff(win_full, A_BOLD | COLOR_PAIR(THEME_PAIR_HEADER));
		mvwprintw(win_full, cy + 3, cx - 12, "[1] Login");
		mvwprintw(win_full, cy + 4, cx - 12, "[2] Register");
		mvwprintw(win_full, cy + 6, cx - 12, "[3] Quit");
		wrefresh(win_full);
		pthread_mutex_unlock(&ui_lock);
		int ch = wgetch(win_full);
		if (ch == KEY_RESIZE)
		{
			ui_resize();
			continue;
		}
		if (ch == '1' || ch == '2' || ch == '3' || ch == 'q')
			return (ch == 'q' ? 3 : ch - '0');
		if (ch == KEY_MOUSE)
		{
			MEVENT e;
			if (getmouse(&e) == OK && (e.bstate & BUTTON1_CLICKED))
			{
				if (e.y == cy + 3)
					return 1;
				if (e.y == cy + 4)
					return 2;
				if (e.y == cy + 6)
					return 3;
			}
		}
	}
}

void ui_draw_first_start_wizard(char *h, int l, int *p)
{
	ui_prompt_input("SETUP", "Host:", h, l - 1);
	char pb[8] = {0};
	ui_prompt_input("SETUP", "Port:", pb, 6);
	if (strlen(h) == 0)
		strcpy(h, "127.0.0.1");
	if (strlen(pb) == 0)
		strcpy(pb, "8010");
	*p = atoi(pb);
}

void ui_prompt_friend_code(char *c)
{
	ui_prompt_input("ADD FRIEND", "Friend Code:", c, 8);
}