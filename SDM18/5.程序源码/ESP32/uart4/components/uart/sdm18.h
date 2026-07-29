#ifndef SMD18_H
#define SMD18_H

#include "stdio.h"

#define u8 uint8_t


#define CRC16_POLYNOMIAL 0x8005
#define bool _Bool
#define true 1
#define false 0

typedef enum sdm18Baud
{
  B9600 ,
	B14400,
	B19200,
	B38400,
	B43000,
	B57600,
	B76800,
	B115200 ,
	B128000,
	B230400,
	B256000,
	B460800,
	B921600 
	

}SDM18_Baud_t;

void SMD18_init(void);
void stop_scan(void);
void start_scan(void);
void SMD18_setbaudrate(SDM18_Baud_t i);
void print_message(void);
void SDM18_Decode(uint8_t RxData);




extern uint8_t send_buf[18];
extern uint8_t Rx_buffer_temp[64];
extern uint8_t Rx_buffer_ok[64];
extern uint8_t Sensor_Data[30];
extern  uint8_t newlines;//1:接收到一包完整的数据 0:没数据	1: Received a complete packet of data 0: No data
extern uint16_t CRC_buff;
#endif
