/**
 * @file    app_debug.c
 * @brief   调试日志和 OLED 显示实现
 */
#include "app_debug.h"
#include "app_safety.h"
#include "app_estimator.h"
#include "app_controller.h"
#include "app_scheduler.h"
#include "bsp_uart_dma.h"
#include "bsp_oled_spi.h"
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * 静态格式化缓冲
 * ---------------------------------------------------------------------- */
static char s_log_buf[128U];

/* -------------------------------------------------------------------------
 * 公开接口实现
 * ---------------------------------------------------------------------- */

void App_Debug_Init(void)
{
    /* USART1 DMA 已在 BSP_UartDma_Init 中初始化 */
}

void App_Debug_Log(const char *module, const char *msg)
{
    int n = snprintf(s_log_buf, sizeof(s_log_buf) - 1U,
                     "[%lu] %s: %s\r\n",
                     (unsigned long)HAL_GetTick(),
                     (module != NULL) ? module : "?",
                     (msg    != NULL) ? msg    : "");
    if (n > 0)
    {
        (void)BSP_UartDma_Transmit(UART_PORT_DEBUG,
                                    (const uint8_t *)s_log_buf,
                                    (uint16_t)n);
    }
}

void App_Debug_UpdateDisplay(void)
{
    estimator_state_t est;
    ctrl_output_t     ctrl;
    safety_state_t    safety;

    App_Estimator_GetState(&est);
    App_Controller_GetOutput(&ctrl);
    safety = App_Safety_GetState();

    BSP_OledSpi_Clear();

    /* 第 0 页：系统状态 */
    static const char * const k_states[] = {"IDLE   ", "RUNNING", "FAULT  "};
    const char *state_str = (safety < 3U) ? k_states[safety] : "?????  ";
    BSP_OledSpi_DrawString(0U, 0U, "SYS:");
    BSP_OledSpi_DrawString(30U, 0U, state_str);

    /* 第 1 页：位置（µm） */
    char line[22U];
    (void)snprintf(line, sizeof(line), "POS:%7ldmm",
                   (long)(est.valid ? est.pos_um / 1000 : 0));
    BSP_OledSpi_DrawString(0U, 1U, line);

    /* 第 2 页：角度（count） */
    (void)snprintf(line, sizeof(line), "ANG:%6ld cnt",
                   (long)(est.valid ? est.angle_mrad : 0));
    BSP_OledSpi_DrawString(0U, 2U, line);

    /* 第 3 页：步频 */
    (void)snprintf(line, sizeof(line), "STP:%5ldHz %c",
                   (long)ctrl.target_step_freq_hz,
                   ctrl.dir_fwd ? '+' : '-');
    BSP_OledSpi_DrawString(0U, 3U, line);

    /* 第 4 页：调度器最大延迟 */
    (void)snprintf(line, sizeof(line), "MAX:%4ldus",
                   (long)App_Scheduler_GetMaxLoopUs());
    BSP_OledSpi_DrawString(0U, 4U, line);

    /* WARN 表示正在降级救场；FAULT 表示已无可用闭环。 */
    (void)snprintf(line, sizeof(line), "WARN:%04lX",
                   (unsigned long)App_Safety_GetWarningMask());
    BSP_OledSpi_DrawString(0U, 5U, line);
    (void)snprintf(line, sizeof(line), "FAULT:%04lX",
                   (unsigned long)App_Safety_GetFaultMask());
    BSP_OledSpi_DrawString(0U, 6U, line);
}
