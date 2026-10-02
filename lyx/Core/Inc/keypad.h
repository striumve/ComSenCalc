#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

/* 两片 TTP229-BSF，每片 16 个触摸通道。
 *
 *   芯片 A : SCL = PB6, SDO = PB7
 *   芯片 B : SCL = PB8, SDO = PB9
 *
 * 每片芯片用到 16 个通道中的 15 个，一共 30 个物理按键，编号 0..29
 * （面板排布见 keymap.h）。
 */
#define KEYPAD_KEY_COUNT 30U

void keypad_init(void);

/* 30 位的物理按键位图：第 i 位为 1 表示第 i 个键被触摸。
 * 没有按键时返回 0。每 10ms 调用一次是安全的；两片芯片连续读出，
 * 整个读取过程大约 0.3ms。 */
uint32_t touch_raw_read(void);

/* 位图中最低的那个置位下标（0..29），位图为 0 时返回 0xFF。 */
uint8_t keypad_first_key(uint32_t bitmap);

/* 仅用于台面调试的接线检查，应用程序不使用。
 * keypad_debug_pin_state: 0=PB6 1=PB7 2=PB8 3=PB9，返回
 *   'Z' = 高阻、'H' = 恒为高、'L' = 恒为低。
 * keypad_debug_short_test: 'S' 表示该芯片的 SCL/SDO 短接在一起。 */
char keypad_debug_pin_state(uint8_t index);
char keypad_debug_short_test(uint8_t chip);

/* 直接从芯片1 或芯片2 读出的原始 16 位帧，未经通道到按键的查表转换。
 * 用来观察一次触摸到底触发了哪些通道。 */
uint16_t keypad_debug_raw(uint8_t chip);

/* 同上，但可以指定 SCL 的半周期（微秒），用于检查结果是否和时钟速度
 * 有关。 */
uint16_t keypad_debug_raw_speed(uint8_t chip, uint16_t half_us);

#endif
