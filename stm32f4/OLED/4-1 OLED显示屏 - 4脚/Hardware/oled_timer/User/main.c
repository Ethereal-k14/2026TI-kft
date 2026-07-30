#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "Key.h"
#include "Timer.h"

#define STATE_IDLE      0
#define STATE_RUNNING   1
#define STATE_STOPPED   2

int main(void)
{
    OLED_Init();
    Key_Init();
    Timer_Init();

    uint8_t state = STATE_IDLE;
    uint32_t start_tick = 0;
    uint32_t elapsed_ms = 0;
    uint32_t last_update = 0;

    OLED_ShowString(1, 4, "RUNTIME");
    OLED_ShowString(2, 4, "00:00.000");
    OLED_ShowString(3, 1, "K1:Start  K2:Rst");
    OLED_ShowString(4, 1, "State: IDLE");

    while (1)
    {
        uint8_t key = Key_GetNum();
        if (key == 1)
        {
            if (state == STATE_IDLE || state == STATE_STOPPED)
            {
                state = STATE_RUNNING;
                start_tick = Timer_GetTick();
                OLED_ShowString(4, 7, "RUN ");
                OLED_ShowString(3, 1, "K1:Stop   K2:Rst");
            }
            else if (state == STATE_RUNNING)
            {
                state = STATE_STOPPED;
                elapsed_ms = Timer_GetTick() - start_tick;
                OLED_ShowString(4, 7, "STOP");
                OLED_ShowString(3, 1, "K1:Start  K2:Rst");
            }
        }
        if (key == 2)
        {
            state = STATE_IDLE;
            elapsed_ms = 0;
            OLED_ShowString(2, 4, "00:00.000");
            OLED_ShowString(4, 7, "IDLE");
            OLED_ShowString(3, 1, "K1:Start  K2:Rst");
        }
        if (state == STATE_RUNNING)
        {
            uint32_t now = Timer_GetTick();
            if (now - last_update >= 100)
            {
                last_update = now;
                elapsed_ms = now - start_tick;
                uint16_t min  = (uint16_t)(elapsed_ms / 60000);
                uint16_t sec  = (uint16_t)((elapsed_ms % 60000) / 1000);
                uint16_t msec = (uint16_t)(elapsed_ms % 1000);
                OLED_ShowNum(2, 4, min / 10, 1);
                OLED_ShowNum(2, 5, min % 10, 1);
                OLED_ShowChar(2, 6, ':');
                OLED_ShowNum(2, 7, sec / 10, 1);
                OLED_ShowNum(2, 8, sec % 10, 1);
                OLED_ShowChar(2, 9, '.');
                OLED_ShowNum(2, 10, msec / 100, 1);
                OLED_ShowNum(2, 11, (msec % 100) / 10, 1);
                OLED_ShowNum(2, 12, msec % 10, 1);
            }
        }
    }
}
