#include "ti_msp_dl_config.h"
#include "stdio.h"
#include "delay.h"
#include "usart.h"
#include "SMD18.h"

extern uint8_t newlines;


int main(void)
{	
	USART_Init();
	printf("waiting for SDM18 start work!\r\n");	
	SMD18_init(B921600);//SDM18初始化	SDM18 Initialization
    while(1)
    {
		//stop_scan();
		if(newlines == 1)
		{
			newlines = 0;//清掉它	Clear it
			//打印信息	Print information
			print_message();
			delay_ms(50);
		}
		
    }
}

