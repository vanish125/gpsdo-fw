#ifndef _INT_H_
#define _INT_H_

#include <stdbool.h>
#include <stdint.h>

extern volatile bool     allow_adjustment;
extern volatile uint32_t frequency;
extern volatile uint32_t num_samples;
extern volatile uint32_t device_uptime;
extern volatile uint32_t last_pps_out;
extern volatile bool     pps_out_up;
extern volatile int8_t   contrast;
extern volatile bool     pps_sync_on;
extern volatile uint32_t pps_sync_delay;
extern volatile uint32_t pps_sync_threshold;
extern volatile int32_t  ppb_frequency;
extern volatile int32_t  ppb_error;
extern volatile int32_t  ppb_correction;
extern volatile int32_t  ppb_millis;
extern volatile int32_t  pps_error;
extern volatile int32_t  pps_millis;
extern volatile uint32_t pps_sync_count;
extern volatile uint8_t  current_state_icon;
extern volatile bool     refresh_screen;
extern volatile bool     sync_pps_out;
extern volatile bool     pps_ppm_auto_sync;
extern volatile bool     pwm_auto_save;
extern volatile bool     update_trend;
extern volatile bool     gps_lock_status;
extern volatile bool     ppb_lock_status;

void update_contrast();

#endif
