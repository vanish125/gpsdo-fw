#ifndef _EEPROM_H_
#define _EEPROM_H_

#include "ee.h"
#include <stdint.h>

typedef struct
{
    uint16_t pwm;
    uint8_t  contrast;
    bool     pps_sync_on;
    uint32_t pps_sync_delay;
    uint32_t pps_sync_threshold;
    bool     pps_ppm_auto_sync;
    bool     pwm_auto_save;
    bool     trend_auto_h;
    bool     trend_auto_v;   
    uint32_t trend_v_scale;
    uint32_t trend_h_scale;
} ee_storage_t;

extern ee_storage_t ee_storage;

#endif
