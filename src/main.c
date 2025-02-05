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
        ee_storage.contrast = 75;
    }
    contrast = ee_storage.contrast;

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

    int contrast_pwm = 0;
    GPIO_PinState contrast_pin_state;
    while (1) {
        uint32_t now = HAL_GetTick();
        if(pps_out_up && now-last_pps_out >= PPS_PULSE_WIDTH)
        {
            HAL_GPIO_WritePin(PPS_OUTPUT_GPIO_Port, PPS_OUTPUT_Pin, 0);
            pps_out_up = false;
        }

        gps_read();
        menu_run();

        contrast_pin_state = (contrast_pwm < (contrast)) ? GPIO_PIN_RESET : GPIO_PIN_SET;
        HAL_GPIO_WritePin(LCD_CONTRAST_GPIO_Port, LCD_CONTRAST_Pin, contrast_pin_state);
        contrast_pwm++;
        if(contrast_pwm>=100)
        {
            contrast_pwm = 0;
        }
    }
}
