#ifndef _MENU_H_
#define _MENU_H_

#include <stdbool.h>

// Char code for sat icons
#define SAT_ICON_1_CODE         '-'//0x7F
#define SAT_ICON_2_CODE         '='//'-'
#define SAT_ICON_3_CODE         0xC6//0x7E
// Char code for no sat icon
#define NO_SAT_ICON_CODE        0x04
#define NO_SAT_STD_ICON_CODE    '!'


bool rotary_get_click();
void menu_run();
void lcd_create_chars();
void init_trend_values();

#endif
