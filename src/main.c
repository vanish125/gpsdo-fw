#include "main.h"
#include "LCD.h"
#include "eeprom.h"
#include "frequency.h"
#include "gps.h"
#include "menu.h"
#include "int.h"
#include "tim.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// All times in ms
#define PPS_PULSE_WIDTH 100
#define VCO_ADJUSTMENT_DELAY 3000

void warmup()
{
    char     buf[9];
    uint32_t warmup_time = 300;
    LCD_Clear();
    while (warmup_time != 0) {
        LCD_Puts(0, 0, " WARMUP");
        sprintf(buf, "  %ld  ", warmup_time);
        LCD_Puts(0, 1, buf);
        warmup_time--;

        for (int i = 0; i < 200; i++) {
            if (rotary_get_click()) {
                warmup_time = 0;
                break;
            }
            HAL_Delay(5);
        }
    }
}

uint8_t lcd_backslash[][8] = { { 0b00000, 0b10000, 0b01000, 0b00100, 0b00010, 0b00001, 0b00000, 0b00000 },
                               { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b10000, 0b00000, 0b00000 },
                               { 0b00000, 0b00000, 0b00000, 0b11000, 0b00100, 0b10100, 0b00000, 0b00000 },
                               { 0b00000, 0b11100, 0b00010, 0b11001, 0b00101, 0b10101, 0b00000, 0b00000 },
                               { 0b00000, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b00000, 0b00000 } };

void gpsdo(void)
{
    HAL_TIM_Base_Start_IT(&htim2);

    EE_Init(&ee_storage, sizeof(ee_storage_t));
    EE_Read();
    if (ee_storage.pwm == 0xffff) {
        ee_storage.pwm = 38000;
    }
    TIM1->CCR2 = ee_storage.pwm;
    if (ee_storage.contrast == 0xff) {
        ee_storage.contrast = 80;
    }
    contrast = ee_storage.contrast;
    update_contrast();
    pps_sync_on = ee_storage.pps_sync_on;
    if (ee_storage.pps_sync_delay == 0xffffffff) {
        ee_storage.pps_sync_delay = 10;
    }
    pps_sync_delay = ee_storage.pps_sync_delay;
    if (ee_storage.pps_sync_threshold == 0xffffffff) {
        ee_storage.pps_sync_threshold = 30000;
    }
    pps_sync_threshold = ee_storage.pps_sync_threshold;
    pps_ppm_auto_sync = ee_storage.pps_ppm_auto_sync;
    pwm_auto_save = ee_storage.pwm_auto_save;

    LCD_Init();

    for (int i = 0; i < 5; i++) {
        LCD_CreateChar(i + 1, lcd_backslash[i]);
    }

    gps_start_it();

    // warmup();

    LCD_Clear();

    HAL_Delay(100);
    frequency_start();

    HAL_TIM_Base_Start(&htim3);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

    while (1) {
        uint32_t now = HAL_GetTick();
        if(pps_out_up && now-last_pps_out >= PPS_PULSE_WIDTH)
        {
            HAL_GPIO_WritePin(PPS_OUTPUT_GPIO_Port, PPS_OUTPUT_Pin, 0);
            pps_out_up = false;
        }
        if (now >= VCO_ADJUSTMENT_DELAY) {
            // Start adjusting the VCO after some time
            frequency_allow_adjustment(true);
        }

        gps_read();
        menu_run();
    }
}
