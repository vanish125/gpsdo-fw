#ifndef _GPS_H_
#define _GPS_H_

#include <stdint.h>
#include <stdbool.h>

#define GPS_DEFAULT_BAUDRATE    9600

extern char     gps_time[];
extern char     gps_date[];
extern char     gps_latitude[];
extern char     gps_longitude[];
extern char     gps_n_s[];
extern char     gps_e_w[];
extern double   gps_msl_altitude;
extern double   gps_geoid_separation;
extern char     gps_hdop[];
extern char     gps_last_frame[];
extern bool     gps_last_frame_changed;
extern uint8_t  num_sats;
extern uint32_t gga_frames;
typedef enum { GPS_MODEL_ATGM336H,  GPS_MODEL_NEO6M, GPS_MODEL_NEOM9N, GPS_MODEL_UNKNOWN } gps_model_type;
extern gps_model_type gps_model;
extern int8_t   gps_time_offset;
extern int8_t   gps_day_offset;
extern bool     gps_us_date_format;

void gps_start_it();
void gps_parse(char* line);
void gps_read();

int	 gps_configure_module_uart(uint32_t baudrate);
void gps_reconfigure_uart(uint32_t baudrate);

#endif
