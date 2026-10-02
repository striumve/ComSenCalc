#ifndef APP_H
#define APP_H

 /*
    数据流：
    KeyTask读取按键 -> controllerTask表达式处理 -> computeTask计算 -> displayQueue -> lcdTask显示
 */

void app_init(void);

void app_heartbeat_task(void);  /* defaultTask      */
void app_key_task(void);        /* KeyTask          */
void app_lcd_task(void);        /* lcdTask          */
void app_compute_task(void);    /* calculatorTask   */
void app_controller_task(void); /* controllerTask   */

#endif
