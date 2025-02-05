#ifndef _GPS_H_
#define _GPS_H_

#include <stdint.h>

extern char     gps_time[];
extern char     num_sats;
extern uint32_t gga_frames;

void gps_start_it();
void gps_parse(char* line);
void gps_read();

#endif
