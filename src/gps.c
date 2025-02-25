#include "gps.h"
#include "LCD.h"
#include "main.h"
#include "stm32f1xx_hal_uart.h"
#include "usart.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GPS_LINE 512

char     gps_line[MAX_GPS_LINE];
char     gps_time[9]      = { '\0' };
char     gps_latitude[9]  = { '\0' };
char     gps_longitude[9] = { '\0' };
char     gps_n_s[2]       = { '\0' };
char     gps_e_w[2]       = { '\0' };
double   gps_msl_altitude;
double   gps_geoid_separation;
char     gps_hdop[9]      = { '\0' };
uint8_t  num_sats         = 0;
uint32_t gga_frames       = 0;
size_t   gps_line_len     = 0;

#define FIFO_BUFFER_SIZE 256

typedef struct {
    uint8_t buffer[FIFO_BUFFER_SIZE];
    size_t  read;
    size_t  write;
} fifo_buffer_t;

typedef enum { FIFO_WRITE, FIFO_READ } fifo_operation;

volatile fifo_buffer_t fifo_buffer_gps  = { 0 };
volatile fifo_buffer_t fifo_buffer_comm = { 0 };

size_t fifo_next(volatile const fifo_buffer_t* fifo, fifo_operation op)
{
    if (op == FIFO_WRITE) {
        return (fifo->write + 1) % FIFO_BUFFER_SIZE;
    } else {
        return (fifo->read + 1) % FIFO_BUFFER_SIZE;
    }
}

bool fifo_write(volatile fifo_buffer_t* fifo, const uint8_t c)
{
    size_t next = fifo_next(fifo, FIFO_WRITE);
    if (next == fifo->read) {
        return false;
    }
    fifo->buffer[fifo->write] = c;
    fifo->write               = next;
    return true;
}

bool fifo_read(volatile fifo_buffer_t* fifo, uint8_t* c)
{
    if (fifo->read == fifo->write) {
        return false;
    }
    *c         = fifo->buffer[fifo->read];
    fifo->read = fifo_next(fifo, FIFO_READ);

    return true;
}

#define GPS_RX_BUFFER_SIZE  20
#define COMM_RX_BUFFER_SIZE 1

volatile uint8_t gps_it_buf[GPS_RX_BUFFER_SIZE];
volatile uint8_t comm_it_buf[COMM_RX_BUFFER_SIZE];

void gps_reconfigure_uart(uint32_t baudrate)
{
    /*HAL_UART_DeInit(&huart3);
    huart3.Instance = USART3;
    huart3.Init.BaudRate = baudrate;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
      Error_Handler();
    }*/
    USART3->CR1 &= ~(USART_CR1_UE);
    USART3->BRR = UART_BRR_SAMPLING16(HAL_RCC_GetPCLK1Freq(),baudrate);
    USART3->CR1 |= USART_CR1_UE;
}


static void gps_start_gps_rx()
{
    if (HAL_UART_Receive_DMA(&huart3, (uint8_t*)gps_it_buf, GPS_RX_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
}
static void gps_start_comm_rx()
{
    if (HAL_UART_Receive_DMA(&huart2, (uint8_t*)comm_it_buf, COMM_RX_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart == &huart3) {
        for (size_t i = 0; i < GPS_RX_BUFFER_SIZE; i++) {
            fifo_write(&fifo_buffer_gps, gps_it_buf[i]);
        }
        gps_start_gps_rx();
    } else if (huart == &huart2) {
        for (size_t i = 0; i < COMM_RX_BUFFER_SIZE; i++) {
            fifo_write(&fifo_buffer_comm, comm_it_buf[i]);
        }
        gps_start_comm_rx();
    }
}

void gps_start_it()
{
    gps_start_gps_rx();
    gps_start_comm_rx();
}

// Maybe use X-CUBE-GNSS here?
void gps_parse(char* line)
{
    if (strstr(line, "GGA") == line+3) {
        char* pch = strtok(line, ",");

        pch = strtok(NULL, ","); // Time

        gps_time[0] = pch[0];
        gps_time[1] = pch[1];
        gps_time[2] = ':';
        gps_time[3] = pch[2];
        gps_time[4] = pch[3];
        gps_time[5] = ':';
        gps_time[6] = pch[4];
        gps_time[7] = pch[5];
        gps_time[8] = '\0';

        pch = strtok(NULL, ","); // Latitude
        if(strstr(pch,".") != NULL)
        {
            uint8_t i = 0;
            uint8_t j = 0;
            bool lead = true;
            while(pch[i] != 0 && j < (sizeof(gps_latitude)-1))
            {
                if(pch[i] != '.' && (pch[i] != '0'||!lead))
                {
                    gps_latitude[j] = pch[i];
                    j++;
                    lead = false;
                }
                i++;
            }
            gps_latitude[j] = 0;
        }
        pch = strtok(NULL, ","); // N/S
        if(strlen(pch)<sizeof(gps_n_s))
        {
            strcpy(gps_n_s,pch);
        }
        pch = strtok(NULL, ","); // Longitude
        if(strstr(pch,".") != NULL)
        {
            uint8_t i = 0;
            uint8_t j = 0;
            bool lead = true;
            while(pch[i] != 0 && j < (sizeof(gps_longitude)-1))
            {
                if(pch[i] != '.' && (pch[i] != '0'||!lead))
                {
                    gps_longitude[j] = pch[i];
                    j++;
                    lead = false;
                }
                i++;
            }
            gps_longitude[j] = 0;
        }
        pch = strtok(NULL, ","); // E/W
        if(strlen(pch)<sizeof(gps_e_w))
        {
            strcpy(gps_e_w,pch);
        }
        strtok(NULL, ","); // Fix

        num_sats = atoi(strtok(NULL, ",")); // Num sats used

        pch = strtok(NULL, ","); // HDOP
        if(strlen(pch)<sizeof(gps_hdop))
        {
            strcpy(gps_hdop,pch);
        }
        
        gps_msl_altitude = atof(strtok(NULL, ",")); // MSL Elevation
        strtok(NULL, ","); // Unit
        gps_geoid_separation = atof(strtok(NULL, ",")); // Geoid Separation
        // strtok(NULL, ","); // Unit

        gga_frames++;
    }
}

// Small risk of overrun here...
#define SEND_BUFFER_SIZE 128
uint8_t send_buf[SEND_BUFFER_SIZE];
uint8_t gps_send_buf[SEND_BUFFER_SIZE];
uint8_t comm_send_buf[SEND_BUFFER_SIZE];
size_t  send_size;

void gps_read()
{
    send_size = 0;
    uint8_t c;
    while (fifo_read(&fifo_buffer_gps, &c)) {
        gps_line[gps_line_len++] = c;
        send_buf[send_size++]    = c;
        if (c == '\n') {
            gps_line[gps_line_len] = '\0';
            gps_parse(gps_line);
            gps_line_len = 0;
            continue;
        }
        if (gps_line_len >= MAX_GPS_LINE) {
            gps_line_len = 0;
            return;
        }
    }

    if (send_size) {
        while (huart2.gState != HAL_UART_STATE_READY)
            ;
        memcpy(gps_send_buf, send_buf, SEND_BUFFER_SIZE);
        HAL_UART_Transmit_IT(&huart2, gps_send_buf, send_size);
    }

    send_size = 0;
    while (fifo_read(&fifo_buffer_comm, &c)) {
        send_buf[send_size++] = c;
    }
    
    if (send_size) {
        while (huart3.gState != HAL_UART_STATE_READY)
            ;
        memcpy(comm_send_buf, send_buf, SEND_BUFFER_SIZE);
        HAL_UART_Transmit_IT(&huart3, comm_send_buf, send_size);
    }
    
}
