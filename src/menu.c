#include "frequency.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "LCD.h"
#include "eeprom.h"
#include "gps.h"
#include "stm32f1xx_hal_gpio.h"
#include "int.h"

/// All times in ms
#define DEBOUNCE_TIME        50
#define SCREEN_REFRESH_TIME  500
#define VCO_ADJUSTMENT_DELAY 3000

volatile uint32_t rotary_down_time      = 0;
volatile uint32_t rotary_up_time        = 0;
volatile bool     rotary_press_detected = 0;
volatile int      menu_printing         = 0;

bool rotary_is_down() { return rotary_down_time > rotary_up_time && (HAL_GetTick() - rotary_down_time) > DEBOUNCE_TIME; }

bool rotary_get_click()
{
    bool is_down = rotary_is_down();

    if (is_down && !rotary_press_detected) {
        rotary_press_detected = true;
        return true;
    } else if (!is_down) {
        rotary_press_detected = false;
    }

    return false;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ROTARY_PRESS_Pin) {
        if (HAL_GPIO_ReadPin(ROTARY_PRESS_GPIO_Port, ROTARY_PRESS_Pin) == GPIO_PIN_SET) {
            rotary_up_time = HAL_GetTick();
        } else {
            rotary_down_time = HAL_GetTick();
        }
    }
}

typedef enum { SCREEN_MAIN, SCREEN_PPB, SCREEN_PWM, SCREEN_GPS, SCREEN_UPTIME, SCREEN_FRAMES, SCREEN_CONTRAST, SCREEN_MAX } menu_screen;
typedef enum { SCREEN_GPS_TIME, SCREEN_GPS_LATITUDE, SCREEN_GPS_LONGITUDE, SCREEN_GPS_ALTITUDE, SCREEN_GPS_GEOID, SCREEN_GPS_SATELITES, SCREEN_GPS_HDOP, SCREEN_GPS_MAX } menu_gps_screen;

static menu_screen current_menu_screen = SCREEN_MAIN;
static menu_gps_screen current_menu_gps_screen = SCREEN_GPS_TIME;
static uint32_t    last_screen_refresh = 0;
static uint8_t     menu_level          = 0;
static uint32_t    last_encoder_value  = 0;             

static void menu_force_redraw() { last_screen_refresh = 0; }

static void menu_draw()
{
    char    screen_buffer[13];
    char    ppb_string[5];
    int32_t ppb;

    switch (current_menu_screen) {
    default:
    case SCREEN_MAIN:
        // Main screen with satellites, ppb and UTC time
        ppb = abs(frequency_get_ppb());

        if (ppb > 999) {
            strcpy(ppb_string, ">=10");
        } else {
            sprintf(ppb_string, "%ld.%02ld", ppb / 100, ppb % 100);
        }

        sprintf(screen_buffer, "%02d %s", num_sats, ppb_string);
        LCD_Puts(1, 0, screen_buffer);
        LCD_Puts(0, 1, gps_time);
        break;
    case SCREEN_PPB:
        // Screen with ppb
        ppb = frequency_get_ppb();
        LCD_Puts(1, 0, "PPB:   ");
        LCD_Puts(0, 1, "        ");
        sprintf(screen_buffer, "%ld.%02d", ppb / 100, abs(ppb) % 100);
        LCD_Puts(0, 1, screen_buffer);
        break;
    case SCREEN_PWM:
        // Screen with current PPM
        LCD_Puts(1, 0, "PWM:   ");
        LCD_Puts(0, 1, "        ");
        sprintf(screen_buffer, "%ld", TIM1->CCR2);
        LCD_Puts(0, 1, screen_buffer);
        break;
    case SCREEN_GPS:
        if(menu_level == 0)
        {
            sprintf(screen_buffer, "GPS:%02d\4", num_sats);
            LCD_Puts(1, 0, screen_buffer);
            LCD_Puts(0, 1, gps_time);
        }
        else
        {
            switch (current_menu_gps_screen)
            {
                default:
                case SCREEN_GPS_TIME:
                    LCD_Puts(1, 0, "Time:");
                    LCD_Puts(0, 1, gps_time);
                    break;
                case SCREEN_GPS_LATITUDE:
                    sprintf(screen_buffer, "Lat.: %s", gps_n_s);
                    LCD_Puts(1, 0, screen_buffer);
                    LCD_Puts(0, 1, gps_latitude);
                    break;
                case SCREEN_GPS_LONGITUDE:
                    sprintf(screen_buffer, "Long.:%s", gps_e_w);
                    LCD_Puts(1, 0, screen_buffer);
                    LCD_Puts(0, 1, gps_longitude);
                    break;
                case SCREEN_GPS_ALTITUDE:
                    {
                        double alt_int = floor(gps_msl_altitude);
                        double alt_frac = (gps_msl_altitude - alt_int)*10;
                        LCD_Puts(1, 0, "Alt.:");
                        sprintf(screen_buffer, "%d.%d", ((int)alt_int), ((int)alt_frac));
                        LCD_Puts(0, 1, screen_buffer);
                    }
                    break;
                case SCREEN_GPS_GEOID:
                    {
                        double geoid_int = floor(gps_geoid_separation);
                        double geoid_frac = (gps_geoid_separation - geoid_int)*10;
                        LCD_Puts(1, 0, "Geoid:");
                        sprintf(screen_buffer, "%d.%d", ((int)geoid_int), ((int)geoid_frac));
                        LCD_Puts(0, 1, screen_buffer);
                    }
                    break;
                case SCREEN_GPS_SATELITES:
                    LCD_Puts(1, 0, "Sat. #:");
                    sprintf(screen_buffer, "%02d", num_sats);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_GPS_HDOP:
                    LCD_Puts(1, 0, "HDOP:");
                    LCD_Puts(0, 1, gps_hdop);
                    break;
            }
        }
        break;
    case SCREEN_UPTIME:
        LCD_Puts(1, 0, "UPTIME:");
        LCD_Puts(0, 1, "        ");
        sprintf(screen_buffer, "%ld", device_uptime);
        LCD_Puts(0, 1, screen_buffer);
        break;
    case SCREEN_FRAMES:
        LCD_Puts(1, 0, "GGA FR:");
        LCD_Puts(0, 1, "        ");
        sprintf(screen_buffer, "%ld", gga_frames);
        LCD_Puts(0, 1, screen_buffer);
        break;
    case SCREEN_CONTRAST:
        LCD_Puts(1, 0, menu_level == 0 ? "CNTRST:":"CNTRST?");
        LCD_Puts(0, 1, "        ");
        sprintf(screen_buffer, "%d", contrast);
        LCD_Puts(0, 1, screen_buffer);
        break;
    }
}

void menu_run()
{

    // Detect rotary encoder value change
    uint32_t new_encoder_value = TIM3->CNT / 2;
    if(new_encoder_value != last_encoder_value)
    {
        if(menu_level == 0)
        {   // Main menu => change menu screen
            menu_screen new_view =  new_encoder_value % SCREEN_MAX;
            if (new_view != current_menu_screen) {
                current_menu_screen = new_view;
                LCD_Clear();
                menu_force_redraw();
            }
        }
        else
        {   // Sub menu
            switch(current_menu_screen)
            {
                case SCREEN_PWM:
                    // Go back to main menu
                    LCD_Clear();
                    menu_force_redraw();
                    menu_level = 0;
                    break;
                case SCREEN_GPS:
                    {
                        // GPS view => change gps menu
                        menu_gps_screen new_view =  new_encoder_value % SCREEN_GPS_MAX;
                        if (new_view != current_menu_gps_screen) {
                            current_menu_gps_screen = new_view;
                            LCD_Clear();
                            menu_force_redraw();
                        }
                    }
                    break;
                case SCREEN_CONTRAST:
                    // Update contrast
                    if(new_encoder_value < last_encoder_value || last_encoder_value == 0)
                    {   // Decrease
                        if(contrast>0)
                            contrast--;
                    }
                    else
                    {
                        if(contrast < 100)
                            contrast++;
                    }
                    LCD_Clear();
                    menu_force_redraw();
                    break;
                default:
                    break;
            }
        }
        last_encoder_value = new_encoder_value;
    }

    uint32_t now = HAL_GetTick();

    if (rotary_get_click()) {
        if (menu_level == 0) {
            switch(current_menu_screen)
            {
                case SCREEN_GPS:
                     current_menu_gps_screen = last_encoder_value % SCREEN_GPS_MAX;
                     /* FALLTHROUGH */
                case SCREEN_PWM:
                case SCREEN_CONTRAST:
                    menu_level = 1;
                    LCD_Clear();
                    break;
                default:
                    break;
            }
        } else {
            switch(current_menu_screen)
            {
                case SCREEN_PWM:
                    ee_storage.pwm = TIM1->CCR2;
                    EE_Write();
                    break;
                case SCREEN_CONTRAST:
                    if(ee_storage.contrast != contrast)
                    {   // Contrast has changed => save it to eeprom
                        ee_storage.contrast = contrast;
                        EE_Write();
                    }
                    break;
                default:
                    break;
            }
            LCD_Clear();
            menu_level = 0;
        }
        menu_force_redraw();
    }

    if (now - last_screen_refresh > SCREEN_REFRESH_TIME) {

        // Move this to some other place, the menu system
        // shouldn't be in charge of this
        if (now >= VCO_ADJUSTMENT_DELAY) {
            // Start adjusting the VCO after some time
            frequency_allow_adjustment(true);
        }

        last_screen_refresh = now;

        // Not very effective spinlock...
        while (menu_printing == 1)
            ;
        menu_printing = 1;

        if (menu_level > 0 && current_menu_screen == SCREEN_PWM) {
            LCD_Puts(0, 0, " PRESS ");
            LCD_Puts(0, 1, "TO SAVE");
        } else {
            menu_draw();
        }

        menu_printing = 0;
    }
}
