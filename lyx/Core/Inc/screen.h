#ifndef SCREEN_H
#define SCREEN_H

#include "main.h"
#include <stdint.h>

#define LCD_PORT        GPIOB
#define LCD_RS_PIN      GPIO_PIN_10
#define LCD_E_PIN       GPIO_PIN_11
#define LCD_D4_PIN      GPIO_PIN_12
#define LCD_D5_PIN      GPIO_PIN_13
#define LCD_D6_PIN      GPIO_PIN_14
#define LCD_D7_PIN      GPIO_PIN_15

#define LCD_COLUMNS     16U
#define LCD_ROWS        2U

/* CGRAM 自定义字符（5x8 点阵）。直接把这些值当普通字符写进 DDRAM 即可。
 *
 * 注意：**故意从 0x01 开始，跳过 0x00**。CGRAM 的字符码是 0x00~0x07，但
 * 0x00 就是 C 字符串的结束符 —— 一旦写进显示缓冲区，所有 str* 函数都会
 * 把它当成字符串结尾，字符会被吃掉。所以槽 0 空着不用。 */
#define LCD_CHAR_PI     0x01U /* π */
#define LCD_CHAR_MUL    0x02U /* × */
#define LCD_CHAR_DIV    0x03U /* ÷ */
#define LCD_CHAR_SQRT   0x04U /* √ */

void screen_init(void);
void screen_clear(void);
void screen_write_lines(const char *line1, const char *line2);

/* 把上面那批自定义字形写进 CGRAM。screen_init() 会自动调用一次。 */
void screen_load_custom_chars(void);

void screen_write_frame(const char *line1, const char *line2,
                        uint8_t cursor_enabled, uint8_t cursor_row,
                        uint8_t cursor_column);

#endif
