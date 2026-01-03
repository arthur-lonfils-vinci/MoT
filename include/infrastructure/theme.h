/**
 * @file theme.h
 * @brief UI Theme definitions.
 */

#ifndef THEME_H
#define THEME_H

#include <ncurses.h>

// Logical Color Pairs
#define THEME_PAIR_NORMAL    1
#define THEME_PAIR_HEADER    2
#define THEME_PAIR_HIGHLIGHT 3
#define THEME_PAIR_USER      4
#define THEME_PAIR_SYSTEM    5
#define THEME_PAIR_ALERT     6
#define THEME_PAIR_DANGER    7
#define THEME_PAIR_INPUT     8 // Field background

// Functions implemented in ui.c
void ui_theme_init(int index);
void ui_theme_cycle(void);
int ui_theme_get_index(void);
const char* ui_theme_get_name(void);

#endif