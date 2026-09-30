#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

/* Two TTP229-BSF 16-channel touch chips make up the keypad.
 * Only 15 channels of each are wired, giving 30 logical keys.
 *
 *   chip #1 : SCL = PB6, SDO = PB7  -> keys  0..14
 *   chip #2 : SCL = PB8, SDO = PB9  -> keys 15..29
 */
#define KEYPAD_KEY_COUNT 30U

void keypad_init(void);

/* Current 30-bit key bitmap: bit i set means key i is being touched.
 * Returns 0 when nothing is pressed. Safe to call every 10 ms.
 * Designed to be fed straight into touch_filter_update(). */
uint32_t touch_raw_read(void);

/* Bench helper: raw 16-bit frame straight from chip 1 or chip 2.
 * Use this to confirm wiring and bit order (chip must be 1 or 2). */
uint16_t keypad_debug_frame(uint8_t chip);

/* Index (0..29) of the lowest set bit, or 0xFF when the bitmap is zero. */
uint8_t keypad_first_key(uint32_t bitmap);

#endif
