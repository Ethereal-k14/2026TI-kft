#include "uart0.h"
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "ring_buffer.h"

static const char *TAG = "UART0";

Ring_Buffer_t uart0_ringbuf;



// 初始化串口0, 波特率为115200 
// Initialize serial port 0, the baud rate is 115200.
void Uart0_Init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    uart_driver_install(UART_NUM_0, RX0_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART0_GPIO_TXD, UART0_GPIO_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

// 通过串口0发送一串数据 Send a string of data through serial port 0
int Uart0_Send_Data(uint8_t* data, uint16_t len)
{
    const int txBytes = uart_write_bytes(UART_NUM_0, data, len);
    return txBytes;
}

// 通过串口0发送一个字节 Send a byte through serial port 0
int Uart0_Send_Byte(uint8_t data)
{
    uint8_t data1 = data;
    const int txBytes = uart_write_bytes(UART_NUM_0, &data1, 1);
    return txBytes;
}


