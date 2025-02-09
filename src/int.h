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
extern volatile int32_t  ppb_frequency;
extern volatile int32_t  ppb_error;
extern volatile int32_t  ppb_correction;
extern volatile int32_t  ppb_millis;

#endif
