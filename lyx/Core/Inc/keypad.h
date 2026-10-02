#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

#define KEYPAD_KEY_COUNT 30U

void keypad_init(void);

/* 第i位为1表示第i个键被触摸 */
uint32_t touch_raw_read(void);

/* 位图中最低的下标, 位图为 0 时返回 0xFF。 */
uint8_t keypad_first_key(uint32_t bitmap);

#endif
