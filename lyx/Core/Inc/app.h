#ifndef APP_H
#define APP_H

#include <stdint.h>

 /*
    数据流：
    KeyTask读取按键 -> controllerTask表达式处理 -> computeTask计算 -> displayQueue -> lcdTask显示
 */

void app_init(void);

/* 由 USB 中断（CDC_Receive_FS）调用，把收到的字节放进环形缓冲区。
 * 在中断上下文里运行，不做任何可能阻塞的操作。 */
void app_usb_rx_push(const uint8_t *data, uint32_t len);

void app_heartbeat_task(void);  /* defaultTask      */
void app_key_task(void);        /* KeyTask          */
void app_lcd_task(void);        /* lcdTask          */
void app_compute_task(void);    /* calculatorTask   */
void app_controller_task(void); /* controllerTask   */

#endif
