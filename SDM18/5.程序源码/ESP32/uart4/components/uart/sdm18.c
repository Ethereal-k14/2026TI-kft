#include <string.h>
#include "sdm18.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "uart1.h"
#include "uart0.h"

uint8_t send_buf[18]={0};


uint8_t Rx_buffer_temp[64];
uint8_t Rx_buffer_ok[64];

uint8_t Sensor_Data[30];
uint8_t newlines = 0;//1:接收到一包完整的数据 0:没数据	1: Received a complete packet of data 0: No data


uint16_t CRC_buff = 0;

uint16_t calculate_crc16(uint8_t *puchMsg, int usDataLen)
{
  uint8_t wChar = 0;
	uint16_t wCRCin = 0xffff; //初始值	Initial value
	uint16_t wCPoly = CRC16_POLYNOMIAL;//多项式	Polynomial
	uint16_t wResultXOR = 0; //结果异或值	Result XOR value
	bool input_invert = true; //输入反转	Input inversion
	bool ouput_invert = true; //输出反转	Output inversion
	
	
    while (usDataLen--)
    {
        wChar = *(puchMsg++);
        if(input_invert)//输入值反转	Input value inversion
        {
            uint8_t temp_char = wChar;
            wChar=0;
            for(int i=0;i<8;++i)
            {
                if(temp_char&0x01)
                    wChar|=0x01<<(7-i);
                temp_char>>=1;
            }
        }
        wCRCin ^= (wChar << 8);
        for (int i = 0; i < 8; i++)
        {
            if (wCRCin & 0x8000)
                wCRCin = (wCRCin << 1) ^ wCPoly;
            else
                wCRCin = wCRCin << 1;
        }
    }
    if(ouput_invert)
    {
        uint16_t temp_short = wCRCin;
        wCRCin=0;
        for(int i=0;i<16;++i)
        {
            if(temp_short&0x01)
                wCRCin|=0x01<<(15-i);
            temp_short>>=1;
        }
    }
    return (wCRCin^wResultXOR);
}




//初始化配置SMD18	Initialize configuration SMD18
void SMD18_init(void)

{
	stop_scan();//停止扫描	Stop Scanning

	//开始扫描	Start Scan
	start_scan();
}

//停止扫描	Stop Scanning
void stop_scan(void)
{
	uint16_t crc_data=0x00;
	send_buf[0] = 0xA5;
	send_buf[1] = 0x03;
	send_buf[2] = 0x20;
	send_buf[3] = 0x02;
	send_buf[4] = 0x00;
	send_buf[5] = 0x00;
	send_buf[6] = 0x00;
	
	crc_data = calculate_crc16(send_buf,7);
	//校验位	Check digit
	send_buf[7] = (crc_data & 0xFF00)>>8; //最高位	Highest bit
	send_buf[8] = crc_data & 0xFF; //最低位	Lowest bit
	
	Uart1_Send_Data(send_buf,9);
	memset(send_buf,0,sizeof(send_buf));
}

//开始扫描	Start Scan
void start_scan(void)
{
	send_buf[0] = 0xA5;
	send_buf[1] = 0x03;
	send_buf[2] = 0x20;
	send_buf[3] = 0x01;
	send_buf[4] = 0x00;
	send_buf[5] = 0x00;
	send_buf[6] = 0x00;
	
	//校验位	Check digit
	send_buf[7] = 0x02;
	send_buf[8] = 0x6E;
	
	Uart1_Send_Data(send_buf,9);
	memset(send_buf,0,sizeof(send_buf));
}


//设置波特率	Setting the baud rate
void SMD18_setbaudrate(SDM18_Baud_t i)
{
	uint16_t crc_data=0x00;
	
	send_buf[0] = 0xA5;
	send_buf[1] = 0x03;
	send_buf[2] = 0x20;
	send_buf[3] = 0x10;
	send_buf[4] = 0x00;
	send_buf[5] = 0x00;
	send_buf[6] = 0x01;
	
	
	switch(i)
	{
		case B9600:send_buf[7] = 0x00;break;
		case B14400:send_buf[7] = 0x01;break;
		case B19200:send_buf[7] = 0x02;break;
		case B38400:send_buf[7] = 0x03;break;
		case B43000:send_buf[7] = 0x04;break;
		
		case B57600:send_buf[7] = 0x05;break;
		case B76800:send_buf[7] = 0x06;break;
		case B115200:send_buf[7] = 0x07;break;
		case B128000:send_buf[7] = 0x08;break;
		case B230400:send_buf[7] = 0x09;break;
		
		case B256000:send_buf[7] = 0x0A;break;
		case B460800:send_buf[7] = 0x0B;break;
		case B921600:send_buf[7] = 0x0C;break;
		
		default :break;
	}
	
	crc_data = calculate_crc16(send_buf,8);
	
	send_buf[8] = (crc_data & 0xFF00)>>8; //最高位	Highest bit
	send_buf[9] = crc_data & 0xFF; //最低位	Lowest bit
	
	Uart1_Send_Data(send_buf,10);
	memset(send_buf,0,sizeof(send_buf));
	
}


//判断接收到的数据包是否完整 -只判断测距数据
//Determine whether the received data packet is complete - only determine the ranging data
void SDM18_Decode(uint8_t RxData)
{
	static uint8_t step_index = 0;
	static uint16_t len = 0;//数据长度 一般是14
	static uint16_t index = 0;
	
	switch(step_index)
	{
		case 0: Rx_buffer_temp[0] = RxData; //0xA5
						if(Rx_buffer_temp[0] == 0xA5)
							step_index++; 
						break; 
						
		case 1: Rx_buffer_temp[1] = RxData;//0x03
						if(Rx_buffer_temp[1] == 0x03)
							step_index++; 
						else
						{
							step_index = 0;//数据不对,重置接收
						}
						break; 
						
						
		case 2: Rx_buffer_temp[2] = RxData;//0x20
						if(Rx_buffer_temp[2] == 0x20)
							step_index++; 
						else
						{
							step_index = 0;//数据不对,重置接收
						}
						break; 
						
		case 3: Rx_buffer_temp[3] = RxData; 
						if(Rx_buffer_temp[3] == 0x01)
							step_index++; 
						else
						{
							step_index = 0;//数据不对,重置接收
						}
						break; 
						
		case 4: Rx_buffer_temp[4] = RxData;  step_index++; break; //0x00
		case 5: Rx_buffer_temp[5] = RxData;  step_index++; break; //数据长度 high
		case 6: Rx_buffer_temp[6] = RxData;  step_index++; //数据长度 LOW
						len = (Rx_buffer_temp[5]<<8 |Rx_buffer_temp[6]) -1;   //从0开始算
						break; 
		case 7: if(index< len) 
						{
							Rx_buffer_temp[7+index] = RxData;
							index ++;
						}
						else //超出数据长度
						{
							step_index++;
							index = 0;//数据段步长归0
						}
						break;
						
		//CRC校验码判断
		case 8: Rx_buffer_temp[8+len] = RxData; step_index++; break;//校验码 high
		case 9: Rx_buffer_temp[9+len] = RxData; step_index++;break;			//校验码 low
		//赋值操作		
		case 10: 
			memcpy(Rx_buffer_ok,Rx_buffer_temp,sizeof(Rx_buffer_temp));
			memset(Rx_buffer_temp,0,sizeof(Rx_buffer_temp));//清空
			
			CRC_buff = Rx_buffer_ok[8+len]<<8 | Rx_buffer_ok[9+len];
		
			index = 0;
			len = 0;
			step_index = 0;
			newlines = 1; //完整的数据
			break;					
		
	}
	
}


//输出距离、强度	Output distance, intensity
void print_message(void)
{
	uint16_t crc_data;
	uint16_t dis = 0;//距离	distance
	uint16_t strength = 0;//强度	strength
	
	//长度计算	Length calculation
	int crclen = (Rx_buffer_ok[5]<<8 |Rx_buffer_ok[6]);
	crclen = crclen +7; //根据协议所得	According to the agreement
	
	crc_data = calculate_crc16(Rx_buffer_ok , crclen);
	
	if(crc_data != CRC_buff)//校验码不对	The check code is incorrect
	{

		 DEBUG_PRINT("----- CRC_ERROR -----");
		return;
	}
	
	dis = Rx_buffer_ok[7+7]<<8 | Rx_buffer_ok[7+6]; //基本固定	Basic fixed
	strength = Rx_buffer_ok[7+11]<<8 | Rx_buffer_ok[7+10];
    // 输出结果  Output results
	printf("Distance: %dmm,Strength: %d\r\n",dis, strength);

}
