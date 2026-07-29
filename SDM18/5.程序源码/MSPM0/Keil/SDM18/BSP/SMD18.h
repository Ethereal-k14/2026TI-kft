#ifndef SMD18_H
#define SMD18_H

#include "ti_msp_dl_config.h"
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

void SMD18_init(SDM18_Baud_t bound);
void stop_scan(void);
void start_scan(void);
void SMD18_setbaudrate(SDM18_Baud_t i);
void print_message(void);
void SDM18_Decode(uint8_t RxData);
#endif

