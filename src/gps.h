#ifndef _GPS_H_
#define _GPS_H_

#include <stdint.h>
#include <stdbool.h>

#define GPS_DEFAULT_BAUDRATE    9600

extern char     gps_time[];
extern char     gps_latitude[];
extern char     gps_longitude[];
extern char     gps_n_s[];
extern char     gps_e_w[];
extern double   gps_msl_altitude;
extern double   gps_geoid_separation;
extern char     gps_hdop[];
extern uint8_t  num_sats;
extern uint32_t gga_frames;
extern bool     gps_is_atgm336h;
extern uint8_t  gps_time_offset;

void gps_start_it();
void gps_parse(char* line);
void gps_read();

int	 gps_configure_atgm336h(uint32_t baudrate);
void gps_reconfigure_uart(uint32_t baudrate);

#endif
