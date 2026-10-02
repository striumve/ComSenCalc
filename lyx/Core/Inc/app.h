#ifndef APP_H
#define APP_H

/* 计算器应用程序。
 *
 * 数据流：
 *
 *   KeyTask ---------> controllerTask ------> computeTask
 *   (TTP229 读取、       (编辑表达式、          (调用引擎库的
 *    滤波、模型识别      光标、Shift)           calculator_evaluate)
 *    -> 按键编号)             |                      |
 *          |                  v                      v
 *          +------------> displayQueue <-------------+
 *                             |
 *                             v
 *                         lcdTask ---> LCD1602
 *
 * app_init() 必须在内核初始化之后、调度器启动之前调用，和其他
 * CMSIS-RTOS2 对象的创建时机保持一致。
 */

void app_init(void);

void app_heartbeat_task(void);  /* defaultTask      */
void app_key_task(void);        /* KeyTask          */
void app_lcd_task(void);        /* lcdTask          */
void app_compute_task(void);    /* calculatorTask   */
void app_controller_task(void); /* controllerTask   */

#endif
