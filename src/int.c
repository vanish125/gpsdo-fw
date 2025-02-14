#include "int.h"
#include "LCD.h"
#include "frequency.h"
#include "tim.h"
#include "menu.h"
#include <stdlib.h>
#include <string.h>

volatile bool     allow_adjustment = false;
volatile uint32_t previous_capture = 0;
volatile uint32_t frequency        = 0;
volatile uint32_t capture          = 0;
volatile uint32_t pps_capture      = 0;
volatile uint32_t num_samples      = 0;
volatile uint32_t timer_overflows  = 0;
volatile uint32_t pps_overflows    = 0;
volatile uint32_t device_uptime    = 0;
volatile uint8_t  first            = 1;
volatile int8_t   contrast         = 0;
volatile bool     pps_sync_on      = false;
volatile uint32_t pps_sync_delay   = 10;
volatile uint32_t pps_sync_threshold = 30000;
volatile uint32_t last_pps         = 0;
volatile uint32_t last_pps_out     = 0;
volatile bool     pps_out_up       = false;
volatile bool     pps_led_toogle   = false;
volatile bool     blink_toggle     = false;
volatile int32_t  ppb_frequency    = 0;
volatile int32_t  ppb_error        = 0;
volatile int32_t  ppb_correction   = 0;
volatile int32_t  ppb_millis       = 0;
volatile int32_t  pps_error        = 0;
volatile int32_t  pps_millis       = 0;
volatile uint32_t pps_shift_count  = 0;
volatile uint32_t pps_sync_count   = 0;
// Icon to shwow at the top right corner of the screen
volatile uint8_t  current_state_icon    = ' ';

const char spinner[]   = "\2\3\4";
uint8_t    pps_spinner = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim == &htim1) {
        timer_overflows++;
        pps_overflows++;
    } else if (htim == &htim2) {
        // PPS output signal
        HAL_GPIO_WritePin(PPS_OUTPUT_GPIO_Port, PPS_OUTPUT_Pin, 1);
        pps_capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
        pps_overflows = 0;
        last_pps_out = HAL_GetTick();
        pps_out_up = true;
        // PPS LED1 blink
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, pps_led_toogle);
        pps_led_toogle = !pps_led_toogle;
        // Update uptime
        device_uptime++;

        if(HAL_GetTick() - last_pps > 1500)
        {
            current_state_icon = blink_toggle ? 5 : ' ';
            blink_toggle = !blink_toggle;
        }
    }
}

// This gets run each time PPS goes high
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {

        capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

        uint32_t current_tick = HAL_GetTick();
        if (allow_adjustment) {
            // Ignore first capture and do a sanity check on elapsed time since previous PPS
            if (!first && current_tick - last_pps < 1300) {
                frequency = capture - previous_capture + /*(TIM1->ARR + 1)*/ 65536 * timer_overflows;

                int32_t current_error = frequency_get_error();

                if (current_error != 0) {
                    // Use error^3 to adjust PWM for larger errors, but preserve sign.
                    // Make even smaller adjustments close to 0.
                    // This is all just guesses and should be investigated more fully.
                    int32_t adjustment = 0;

                    if (abs(current_error) > 10) {
                        adjustment = abs(current_error) * current_error * 2;
                    } else if (abs(current_error) > 2) {
                        adjustment = abs(current_error) * current_error;
                    } else {
                        adjustment = current_error;
                    }

                    // Apply it
                    TIM1->CCR2 -= adjustment;

                    ppb_correction = -adjustment;
                }
                else {
                    ppb_correction = 0;
                }
                // See if we need to resync MCU PPS Out
                pps_error = (capture - pps_capture + /*(TIM1->ARR + 1)*/ 65536 * pps_overflows) - 70000000 /*HAL_RCC_GetHCLKFreq()*/;
                if(pps_sync_on && (abs(pps_error) >= pps_sync_threshold))
                {
                    pps_shift_count++;
                    if(pps_shift_count > pps_sync_delay)
                    {   // Force sync by reseting TIM2
                        TIM2->CNT = TIM2->ARR;
                        pps_sync_count++;
                    }
                }
                else
                {   // Reset shift count if we are below threshold
                    pps_shift_count = 0;
                }
                // Save values for ppb and pps display
                ppb_frequency = frequency;
                ppb_error = current_error;
                ppb_millis = current_tick - last_pps - 1000;
                pps_millis = (pps_error/7); // Clock is 70 MHz and we want the value in 10s of microseconds so 10 0000 000 / 70 000 000 = 1/7

                circbuf_add(&circular_buffer, current_error);
                if (num_samples < CIRCULAR_BUFFER_LEN)
                    num_samples++;
            }

            previous_capture = capture;
            timer_overflows  = 0;
            first            = 0;
        }
        // Update last PPS time
        last_pps         = current_tick;
        // Update state icon
        current_state_icon = spinner[pps_spinner];
        pps_spinner   = (pps_spinner + 1) % strlen(spinner);
    }
}
