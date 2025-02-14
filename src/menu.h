#ifndef _MENU_H_
#define _MENU_H_

#include <stdbool.h>

extern volatile uint8_t current_state_icon;

bool rotary_get_click();
void menu_run();

#endif
