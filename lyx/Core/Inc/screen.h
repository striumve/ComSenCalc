#ifndef SCREEN_H
#define SCREEN_H

#include "main.h"
#include <stdint.h>

/* LCD1602 四位总线。这几个宏要和板子的实际接线保持一致。 */
#define LCD_PORT        GPIOB
#define LCD_RS_PIN      GPIO_PIN_10
#define LCD_E_PIN       GPIO_PIN_11
#define LCD_D4_PIN      GPIO_PIN_12
#define LCD_D5_PIN      GPIO_PIN_13
#define LCD_D6_PIN      GPIO_PIN_14
#define LCD_D7_PIN      GPIO_PIN_15

#define LCD_COLUMNS     16U
#define LCD_ROWS        2U

void screen_init(void);
void screen_clear(void);
void screen_write_lines(const char *line1, const char *line2);

/* 和 screen_write_lines 相同，但同时定位（或隐藏）硬件光标。
 * cursor_row 取 0 或 1，cursor_column 取 0..15。 */
void screen_write_frame(const char *line1, const char *line2,
                        uint8_t cursor_enabled, uint8_t cursor_row,
                        uint8_t cursor_column);

#endif
