#ifndef _MENU_H_
#define _MENU_H_

#include <stdbool.h>

// Char code for sat icons
#define SAT_ICON_1_CODE     '('
#define SAT_ICON_2_CODE     'o'
#define SAT_ICON_3_CODE     ')'
// Char code for no sat icon
#define NO_SAT_ICON_CODE    'X'


bool rotary_get_click();
void menu_run();
void lcd_create_chars();

#endif
