#include "usart.h"
#include "stdio.h"

#define RE_0_BUFF_LEN_MAX	128

volatile uint8_t  recv0_buff[RE_0_BUFF_LEN_MAX] = {0};
volatile uint16_t recv0_length = 0;
volatile uint8_t  recv0_flag = 0;

void USART_Init(void)
{
	// SYSCFG初始化
	// SYSCFG initialization
	SYSCFG_DL_init();
	//清除串口中断标志
	//Clear the serial port interrupt flag
	NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
	//使能串口中断
	//Enable serial port interrupt
	NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
	
	NVIC_ClearPendingIRQ(UART_1_INST_INT_IRQN);
	NVIC_EnableIRQ(UART_1_INST_INT_IRQN);

}

//串口发送一个字节
//The serial port sends a byte
void USART_SendData(unsigned char data)
{
	//当串口0忙的时候等待
	//Wait when serial port 0 is busy
	while( DL_UART_isBusy(UART_0_INST) == true );
	//发送
	//send
	DL_UART_Main_transmitData(UART_0_INST, data);
}

//USART1发送一个字符
//USART1 sends a character
void USART1_Send_U8(uint8_t data)
{
	//当串口1忙的时候等待
	//Wait when serial port 1 is busy
	while( DL_UART_isBusy(UART_1_INST) == true );
	//发送 Send
	DL_UART_Main_transmitData(UART_1_INST, data);;
}

//串口1发送N个字符
//Serial port 1 sends N characters
void USART1_Send_ArrayU8(uint8_t *pData, uint16_t Length)
{
	while (Length--)
	{
		USART1_Send_U8(*pData);
		pData++;
	}
}


#if !defined(__MICROLIB)
//不使用微库的话就需要添加下面的函数
//If you don't use the micro library, you need to add the following function
#if (__ARMCLIB_VERSION <= 6000000)
//如果编译器是AC5  就定义下面这个结构体
//If the compiler is AC5, define the following structure
struct __FILE
{
	int handle;
};
#endif

FILE __stdout;

//定义_sys_exit()以避免使用半主机模式
//Define _sys_exit() to avoid using semihosting mode
void _sys_exit(int x)
{
	x = x;
}
#endif


//printf函数重定义
//printf function redefinition
int fputc(int ch, FILE *stream)
{
	//当串口0忙的时候等待，不忙的时候再发送传进来的字符
	//Wait when serial port 0 is busy, and send the incoming characters when it is not busy
	while( DL_UART_isBusy(UART_0_INST) == true );
	
	DL_UART_Main_transmitData(UART_0_INST, ch);
	
	return ch;
}

//串口的中断服务函数
//Serial port interrupt service function
void UART_0_INST_IRQHandler(void)
{
	uint8_t receivedData = 0;
	
	//如果产生了串口中断
	//If a serial port interrupt occurs
	switch( DL_UART_getPendingInterrupt(UART_0_INST) )
	{
		case DL_UART_IIDX_RX://如果是接收中断	If it is a receive interrupt
			
			// 接收发送过来的数据保存	Receive and save the data sent
			receivedData = DL_UART_Main_receiveData(UART_0_INST);
		
			if((recv0_length&0x8000)==0)//接收未完成	Receiving is not completed
			{
			if(recv0_length&0x4000)//接收到了0x0d	Received 0x0d
				{
				if(receivedData!=0x0a)recv0_length=0;//接收错误,重新开始	Receive error, restart
				else recv0_length|=0x8000;	//接收完成了 Receiving completed
				}
			else //还没收到0X0D	Haven't received 0X0D yet
				{	
				if(receivedData==0x0d)recv0_length|=0x4000;
				else
					{
					recv0_buff[recv0_length&0X3FFF]=receivedData ;
					recv0_length++;
					if(recv0_length>(RE_0_BUFF_LEN_MAX-1))recv0_length=0;//接收数据错误,重新开始接收	Receive data error, restart receiving  
					}		 
				}
			}   	
		
			break;
		
		default://其他的串口中断	Other serial port interrupts
			break;
	}
}

//串口1的中断服务函数
//Interrupt service function of serial port 1
void UART_1_INST_IRQHandler(void)
{
	uint8_t receivedData = 0;
	
	//如果产生了串口中断
	//If a serial port interrupt occurs
	switch( DL_UART_getPendingInterrupt(UART_1_INST) )
	{
		case DL_UART_IIDX_RX://如果是接收中断	If it is a receive interrupt
			
		// 接收发送过来的数据保存
		//Receive and save the data sent
		receivedData = DL_UART_Main_receiveData(UART_1_INST);
		SDM18_Decode(receivedData);
		break;
		
		default://其他的串口中断	Other serial port interrupts
			break;
	}
}

