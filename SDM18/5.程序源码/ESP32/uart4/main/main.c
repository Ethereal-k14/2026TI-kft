#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "ring_buffer.h"
#include "uart0.h"
#include "uart1.h"
#include "sdm18.h"


void app_main() {
    // 初始化UART uart init 
    Uart0_Init();  // 调试串口 debug uart
    Uart1_Init(921600);  // SDM18通信 sdm18 uart
    printf("SDM18 initializing...\r\n");
    SMD18_init(); // 初始化SMD18 sdm18 init
    
    while(1) {
        // 读取串口1数据 Read serial port 1 data
        uint8_t rxdata;
        //读取数据非空后开机进入状态机解析数据  After reading the data and finding it non-empty, the machine is turned on to enter the state machine for data parsing
        int length = uart_read_bytes(UART_NUM_1, &rxdata, 1, 1/921600);
        if (length > 0) {
            // 解析数据 Parse data
                SDM18_Decode(rxdata);
                if (newlines == 1) {
                    newlines = 0;
                    print_message();    
                }


        vTaskDelay(pdMS_TO_TICKS(1));    
        }
        
    }
}