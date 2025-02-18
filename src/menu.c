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
#include "menu.h"

/// All times in ms
#define DEBOUNCE_TIME        50

// Firmware version tag
#define FIRMWARE_VERSION    "v0.1.4"

volatile uint32_t rotary_down_time      = 0;
volatile uint32_t rotary_up_time        = 0;
volatile bool     rotary_press_detected = 0;



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

uint8_t lcd_backslash[][8] = { { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b10000, 0b00000, 0b00000 },
                               { 0b00000, 0b00000, 0b00000, 0b11000, 0b00100, 0b10100, 0b00000, 0b00000 },
                               { 0b00000, 0b11100, 0b00010, 0b11001, 0b00101, 0b10101, 0b00000, 0b00000 } };

void lcd_create_chars()
{
    for (int i = 0; i < 3; i++) {
        LCD_CreateChar(i+1, lcd_backslash[i]);
    }
}

typedef enum { SCREEN_MAIN, SCREEN_TREND, SCREEN_PPB, SCREEN_PWM, SCREEN_GPS, SCREEN_UPTIME, SCREEN_FRAMES, SCREEN_CONTRAST, SCREEN_PPS, SCREEN_VERSION, SCREEN_MAX } menu_screen;
typedef enum { SCREEN_GPS_TIME, SCREEN_GPS_LATITUDE, SCREEN_GPS_LONGITUDE, SCREEN_GPS_ALTITUDE, SCREEN_GPS_GEOID, SCREEN_GPS_SATELITES, SCREEN_GPS_HDOP, SCREEN_GPS_MAX } menu_gps_screen;
typedef enum { SCREEN_PPB_MEAN, SCREEN_PPB_INST, SCREEN_PPB_FREQUENCY, SCREEN_PPB_ERROR, SCREEN_PPB_CORRECTION, SCREEN_PPB_MILLIS, SCREEN_PPB_AUTO_SAVE_PWM, SCREEN_PPB_AUTO_SYNC_PPS, SCREEN_PPB_MAX } menu_ppb_screen;
typedef enum { SCREEN_PPS_SHIFT, SCREEN_PPS_SHIFT_MS, SCREEN_PPS_SYNC_COUNT, SCREEN_PPS_SYNC_MODE, SCREEN_PPS_SYNC_DELAY, SCREEN_PPS_SYNC_THRESHOLD, SCREEN_PPS_FORCE_SYNC, SCREEN_PPS_MAX } menu_pps_screen;

static menu_screen current_menu_screen = SCREEN_MAIN;
static menu_gps_screen current_menu_gps_screen = SCREEN_GPS_TIME;
static menu_ppb_screen current_menu_ppb_screen = SCREEN_PPB_MEAN;
static menu_pps_screen current_menu_pps_screen = SCREEN_PPS_SHIFT;
static uint8_t     menu_level          = 0;
static uint32_t    last_encoder_value  = 0;
static bool        auto_save_pwm_done  = false;
static bool        auto_sync_pps_done  = false;

static uint8_t     trend_pos = 0;

static void menu_force_redraw() { refresh_screen = true; }

static void menu_draw_trend()
{
    uint8_t values[40];
    for(int i = 0 ; i < 40 ; i++)
    {
        //values[i] = (uint8_t)((((sin((6.28318530718*i/*+(trend_pos*6.28318530718/39)*/))/39)+1)*7)/2);
        values[i] = (uint8_t)(((sin((6.28318530718*(i+trend_pos)/39))+1)*8)/2);
        //values[i] = i%8;
    }
    for(int col_screen = 0 ; col_screen < 8 ; col_screen++)
    {
        uint8_t cust_char[8] = {0};
        for(int col_char = 0; col_char < 5 ; col_char++)
        {
            uint8_t cur_val = values[col_screen * 5 + col_char];
            cust_char[7-cur_val]  |= (0b10000 >> col_char);
        }
        LCD_CreateChar(col_screen,cust_char);
        LCD_PutCustom(col_screen,1,col_screen);
    }
    trend_pos++;
    if(trend_pos > 39)
    {
        trend_pos = 0;
    }
}

static void menu_format_ppb(char* ppb_string)
{
    int32_t ppb = abs(frequency_get_ppb());

    if (ppb ==  0xFFFF) {
        strcpy(ppb_string, "   ?");
    } else if (ppb > 999999) {
        strcpy(ppb_string, ">10k");
    } else if (ppb > 9999) {
        sprintf(ppb_string, "%4ld", (ppb / 100));
    } else if (ppb > 999) {
        sprintf(ppb_string, "%ld.%01ld", ppb / 100, ((ppb % 100)/10));
    } else {
        sprintf(ppb_string, "%ld.%02ld", ppb / 100, ppb % 100);
    }
}

static void menu_draw()
{
    char    screen_buffer[14];
    char    ppb_string[5];
    int32_t ppb;

    switch (current_menu_screen) {
    default:
    case SCREEN_MAIN:
        // Main screen with satellites, ppb and UTC time
        menu_format_ppb(ppb_string);
        sprintf(screen_buffer, "%02d %s", num_sats, ppb_string);
        LCD_Puts(1, 0, screen_buffer);
        LCD_Puts(0, 1, gps_time);
        break;
    case SCREEN_TREND:
        // Trend screen 
        menu_format_ppb(ppb_string);
        sprintf(screen_buffer, "%02d %s", num_sats, ppb_string);
        LCD_Puts(1, 0, screen_buffer);
        menu_draw_trend();
        break;
    case SCREEN_PPB:
        // Screen with ppb
        if(menu_level == 0)
        {
            ppb = frequency_get_ppb();
            LCD_Puts(1, 0, "PPB:   ");
            LCD_Puts(0, 1, "        ");
            sprintf(screen_buffer, "%ld.%02d", ppb / 100, abs(ppb) % 100);
            LCD_Puts(0, 1, screen_buffer);
        }
        else
        {
            // Clear line 2
            LCD_Puts(0, 1, "        ");
            switch (current_menu_ppb_screen)
            {
                default:
                case SCREEN_PPB_MEAN:
                    ppb = frequency_get_ppb();
                    LCD_Puts(1, 0, "Mean:");
                    sprintf(screen_buffer, "%ld.%02d", ppb / 100, abs(ppb) % 100);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPB_INST:
                    {
                    LCD_Puts(1, 0, "Inst:");
                    int32_t ppb_inst = (int64_t)ppb_error * 1000000000 * 100 / ((int64_t)HAL_RCC_GetHCLKFreq());
                    sprintf(screen_buffer, "%ld.%02d", ppb_inst / 100, abs(ppb_inst) % 100);
                    LCD_Puts(0, 1, screen_buffer);
                    }
                    break;
                case SCREEN_PPB_FREQUENCY:
                    LCD_Puts(1, 0, "Freq:");
                    sprintf(screen_buffer, "%ld", ppb_frequency);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPB_ERROR:
                    LCD_Puts(1, 0, "Error:");
                    sprintf(screen_buffer, "%ld", ppb_error);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPB_CORRECTION:
                    LCD_Puts(1, 0, "Corr.:");
                    sprintf(screen_buffer, "%ld", ppb_correction);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPB_MILLIS:
                    LCD_Puts(1, 0, "Millis:");
                    sprintf(screen_buffer, "%ld", ppb_millis);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPB_AUTO_SAVE_PWM:
                    LCD_Puts(1, 0, menu_level == 1 ? "PWM S.:":"PWM S.?");
                    LCD_Puts(0, 1, pwm_auto_save ? "      ON" : "     OFF");
                    break;
                case SCREEN_PPB_AUTO_SYNC_PPS:
                    LCD_Puts(1, 0, menu_level == 1 ? "PPS S.:":"PPS S.?");
                    LCD_Puts(0, 1, pps_ppm_auto_sync ? "      ON" : "     OFF");
                    break;
            }
        }
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
            // Clear line 2
            LCD_Puts(0, 1, "        ");
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
    case SCREEN_PPS:
        // Screen with pps
        // Clear line 2
        LCD_Puts(0, 1, "        ");
        if(menu_level == 0)
        {
            sprintf(screen_buffer, "PPS:%3ld", pps_sync_count);
            LCD_Puts(1, 0, screen_buffer);
            sprintf(screen_buffer, "%ld", pps_error);
            LCD_Puts(0, 1, screen_buffer);
        }
        else
        {
            switch (current_menu_pps_screen)
            {
                default:
                case SCREEN_PPS_SHIFT:
                    LCD_Puts(1, 0, "Shift:");
                    // Check we have enough space for minus sign
                    sprintf(screen_buffer, "%ld", (pps_error < -9999999) ? abs(pps_error) : pps_error);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPS_SHIFT_MS:
                    LCD_Puts(1, 0, "Sft ms:");
                    sprintf(screen_buffer, "%ld.%04d", pps_millis / 10000, abs(pps_millis) % 10000);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPS_SYNC_COUNT:
                    LCD_Puts(1, 0, "SynCnt:");
                    sprintf(screen_buffer, "%ld", pps_sync_count);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPS_SYNC_MODE:
                    LCD_Puts(1, 0, menu_level == 1 ? "Sync.:":"Sync.?");
                    LCD_Puts(0, 1, pps_sync_on ? "      ON" : "     OFF");
                    break;
                case SCREEN_PPS_SYNC_DELAY:
                    LCD_Puts(1, 0, menu_level == 1 ? "Delay:":"Delay?");
                    sprintf(screen_buffer, "%ld", pps_sync_delay);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPS_SYNC_THRESHOLD:
                    LCD_Puts(1, 0, menu_level == 1 ? "Thrsld:":"Thrsld?");
                    sprintf(screen_buffer, "%ld", pps_sync_threshold);
                    LCD_Puts(0, 1, screen_buffer);
                    break;
                case SCREEN_PPS_FORCE_SYNC:
                    if(menu_level == 1)
                    {
                        LCD_Puts(1, 0,  " Force ");
                        LCD_Puts(0, 1, "  sync ?");
                    }
                    else
                    {
                        LCD_Puts(1, 0,  " Forced");
                        LCD_Puts(0, 1, "  sync !");
                        sync_pps_out = true;
                        menu_level = 1;
                    }
                    break;
            }
        }
        break;
    case SCREEN_VERSION:
        LCD_Puts(1, 0, "Vers.:");
        LCD_Puts(0, 1, FIRMWARE_VERSION);
        break;
    }
}

void menu_run()
{

    // Detect rotary encoder value change
    uint32_t new_encoder_value = TIM3->CNT / 2;
    if(new_encoder_value != last_encoder_value)
    {
        menu_screen previous_menu_screen = current_menu_screen;
        int encoder_increment = (new_encoder_value < last_encoder_value)? -1 : +1;
        // Handle overflow cases
        if(new_encoder_value == 32767 && last_encoder_value == 0)
        {
            encoder_increment = -1;
        }
        else if (new_encoder_value == 0 && last_encoder_value == 32767)
        {
            encoder_increment = +1;
        }
        if(menu_level == 0)
        {   // Main menu => change menu screen
            current_menu_screen =  (current_menu_screen + encoder_increment) % SCREEN_MAX;
            if(current_menu_screen >= SCREEN_MAX) current_menu_screen = SCREEN_MAX-1; // Roll over for first sceen - 1
            LCD_Clear();
            menu_force_redraw();
        }
        else if(menu_level == 1)
        {   // Sub menu
            switch(current_menu_screen)
            {
                case SCREEN_PWM:
                    // Go back to main menu
                    LCD_Clear();
                    menu_force_redraw();
                    menu_level = 0;
                    break;
                case SCREEN_PPB:
                    {
                        // PPB view => change ppb menu
                        current_menu_ppb_screen =  (current_menu_ppb_screen + encoder_increment) % SCREEN_PPB_MAX;
                        if(current_menu_ppb_screen >= SCREEN_PPB_MAX) current_menu_ppb_screen = SCREEN_PPB_MAX-1; // Roll over for first sceen - 1
                        LCD_Clear();
                        menu_force_redraw();
                    }
                    break;
                case SCREEN_GPS:
                    {
                        // GPS view => change gps menu
                        current_menu_gps_screen =  (current_menu_gps_screen + encoder_increment) % SCREEN_GPS_MAX;
                        if(current_menu_gps_screen >= SCREEN_GPS_MAX) current_menu_gps_screen = SCREEN_GPS_MAX-1; // Roll over for first sceen - 1
                        LCD_Clear();
                        menu_force_redraw();
                    }
                    break;
                case SCREEN_CONTRAST:
                    // Update contrast
                    contrast += encoder_increment;
                    if(contrast < 0) contrast = 0;
                    if(contrast > 100) contrast = 100;
                    update_contrast();
                    LCD_Clear();
                    menu_force_redraw();
                    break;
                case SCREEN_PPS:
                    {
                        // PPB view => change ppb menu
                        current_menu_pps_screen =  (current_menu_pps_screen + encoder_increment) % SCREEN_PPS_MAX;
                        if(current_menu_pps_screen >= SCREEN_PPS_MAX) current_menu_pps_screen = SCREEN_PPS_MAX-1; // Roll over for first sceen - 1
                        LCD_Clear();
                        menu_force_redraw();
                    }
                    break;
                default:
                    break;
            }
        }
        else if(menu_level == 2 && current_menu_screen == SCREEN_PPB)
        {   // Sub-sub menu for PPB screen
            switch(current_menu_ppb_screen)
            {
                case SCREEN_PPB_AUTO_SAVE_PWM:
                    // Update mode
                    pwm_auto_save = !pwm_auto_save;
                    LCD_Clear();
                    menu_force_redraw();
                    break;
                case SCREEN_PPB_AUTO_SYNC_PPS:
                    // Update mode
                    pps_ppm_auto_sync = !pps_ppm_auto_sync;
                    LCD_Clear();
                    menu_force_redraw();
                    break;
                default:
                    break;
            }
        }
        else if(menu_level == 2 && current_menu_screen == SCREEN_PPS)
        {   // Sub-sub menu for PPS screen
            switch(current_menu_pps_screen)
            {
                case SCREEN_PPS_SYNC_MODE:
                    // Update mode
                    pps_sync_on = !pps_sync_on;
                    LCD_Clear();
                    menu_force_redraw();
                    break;
                case SCREEN_PPS_SYNC_DELAY:
                    // Update delay
                    pps_sync_delay += encoder_increment;
                    LCD_Clear();
                    menu_force_redraw();
                    break;
                case SCREEN_PPS_SYNC_THRESHOLD:
                    // Update delay
                    pps_sync_threshold += encoder_increment;
                    LCD_Clear();
                    menu_force_redraw();
                    break;
                case SCREEN_PPS_FORCE_SYNC:
                    // PPB view => change ppb menu
                    current_menu_pps_screen =  (current_menu_pps_screen + encoder_increment) % SCREEN_PPS_MAX;
                    if(current_menu_pps_screen >= SCREEN_PPS_MAX) current_menu_pps_screen = SCREEN_PPS_MAX-1; // Roll over for first sceen - 1
                    LCD_Clear();
                    menu_force_redraw();
                    break;
                default:
                    break;
            }
        }
        if(previous_menu_screen == SCREEN_TREND)
        {   // After trend screen, restore custom icon chars
            lcd_create_chars();
        }
        last_encoder_value = new_encoder_value;
    }


    if (rotary_get_click()) {
        if (menu_level == 0) {
            switch(current_menu_screen)
            {
                case SCREEN_PPB:
                case SCREEN_GPS:
                case SCREEN_PWM:
                case SCREEN_CONTRAST:
                case SCREEN_PPS:
                    menu_level = 1;
                    LCD_Clear();
                    break;
                default:
                    break;
            }
        } else  if (menu_level == 1){
            switch(current_menu_screen)
            {
                case SCREEN_PWM:
                    ee_storage.pwm = TIM1->CCR2;
                    EE_Write();
                    menu_level = 0;
                    break;
                case SCREEN_CONTRAST:
                    if(ee_storage.contrast != contrast)
                    {   // Contrast has changed => save it to eeprom
                        ee_storage.contrast = contrast;
                        EE_Write();
                    }
                    menu_level = 0;
                    break;
                case SCREEN_PPB:
                    switch(current_menu_ppb_screen)
                    {
                        case SCREEN_PPB_AUTO_SAVE_PWM:
                        case SCREEN_PPB_AUTO_SYNC_PPS:
                            menu_level = 2;
                            break;
                        default:
                            menu_level = 0;
                            break;
                    }
                    break;
                case SCREEN_PPS:
                    switch(current_menu_pps_screen)
                    {
                        case SCREEN_PPS_SYNC_MODE:
                        case SCREEN_PPS_SYNC_DELAY:
                        case SCREEN_PPS_SYNC_THRESHOLD:
                        case SCREEN_PPS_FORCE_SYNC:
                            menu_level = 2;
                            break;
                        default:
                            menu_level = 0;
                            break;
                    }
                    break;
                default:
                    menu_level = 0;
                    break;
            }
            LCD_Clear();
        } else  if (menu_level == 2 && current_menu_screen == SCREEN_PPB){
            switch(current_menu_ppb_screen)
            {
                case SCREEN_PPB_AUTO_SAVE_PWM:
                    if(ee_storage.pwm_auto_save != pwm_auto_save)
                    {   // Save changes
                        ee_storage.pwm_auto_save = pwm_auto_save;
                        EE_Write();
                    }
                    break;
                case SCREEN_PPB_AUTO_SYNC_PPS:
                    if(ee_storage.pps_ppm_auto_sync != pps_ppm_auto_sync)
                    {   // Save changes
                        ee_storage.pps_ppm_auto_sync = pps_ppm_auto_sync;
                        EE_Write();
                    }
                    break;
                default:
                    break;
            }
            menu_level = 1;
            LCD_Clear();
        } else  if (menu_level == 2 && current_menu_screen == SCREEN_PPS){
            switch(current_menu_pps_screen)
            {
                case SCREEN_PPS_SYNC_MODE:
                    if(ee_storage.pps_sync_on != pps_sync_on)
                    {   // Save changes
                        ee_storage.pps_sync_on = pps_sync_on;
                        EE_Write();
                    }
                    break;
                case SCREEN_PPS_SYNC_DELAY:
                    if(ee_storage.pps_sync_delay != pps_sync_delay)
                    {   // Save changes
                        ee_storage.pps_sync_delay = pps_sync_delay;
                        EE_Write();
                    }
                    break;
                case SCREEN_PPS_SYNC_THRESHOLD:
                    if(ee_storage.pps_sync_threshold != pps_sync_threshold)
                    {   // Save changes
                        ee_storage.pps_sync_threshold = pps_sync_threshold;
                        EE_Write();
                    }
                    break;
                default:
                    break;
            }
            menu_level = 1;
            LCD_Clear();
        }
        else
        {
            menu_level = 0;
            LCD_Clear();
        }
        menu_force_redraw();
    }

    if (refresh_screen) {
        refresh_screen = false;

        // Display state icon
        if(current_menu_screen == SCREEN_TREND && (current_state_icon < 8))
        {   // Don't use custom icon in trend screen since all 8 custom chars are used for graphic display
            switch (current_state_icon)
            {
                default:
                case 0:
                    current_state_icon = SAT_ICON_1_CODE;
                    break;
                case 1:
                    current_state_icon = SAT_ICON_2_CODE;
                    break;
                case 2:
                    current_state_icon = SAT_ICON_3_CODE;
                    break;
            }
        }
        LCD_PutCustom(0,0,current_state_icon);

        if (menu_level > 0 && current_menu_screen == SCREEN_PWM) {
            LCD_Puts(0, 0, " PRESS ");
            LCD_Puts(0, 1, "TO SAVE");
        } else {
            menu_draw();
        }

        // Check if we need resync or PWM save
        if(frequency_is_stable())
        {   // Frequency is stabilized
            // Save PWM if requested
            bool did_pwm = false;
            bool did_pps = false;
            if(pwm_auto_save && !auto_save_pwm_done)
            {
                ee_storage.pwm = TIM1->CCR2;
                EE_Write();
                // Only auto-save once per session
                auto_save_pwm_done = true;
                did_pwm = true;
            }
            if(pps_ppm_auto_sync && !auto_sync_pps_done)
            {
                sync_pps_out = true;
                // Only auto-sync once per session
                auto_sync_pps_done = true;
                did_pps = true;
            }
            if(did_pps && did_pwm)
            {
                LCD_Puts(0, 0, "PPS&PWM ");
                LCD_Puts(0, 1, " DONE ! ");
            }
            else if(did_pps)
            {
                LCD_Puts(0, 0, "  PPS  ");
                LCD_Puts(0, 1, "SYNCED!");
            }
            else if(did_pwm)
            {
                LCD_Puts(0, 0, "  PWM  ");
                LCD_Puts(0, 1, "SAVED !");
            }
        }
    }
}
