#ifndef	__USART_H__
#define __USART_H__

#include "ti_msp_dl_config.h"
#include "SMD18.h"


void USART_Init(void);

void USART_SendData(unsigned char data);
void USART1_Send_ArrayU8(uint8_t *pData, uint16_t Length);

#endif
