#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>

/* 
  Shift   Back    Mode    上      OK
  (       )       左      下      右
  7       8       9       DEL     AC
  4       5       6       *       /
  1       2       3       +       -
  0       .       10^     FMT     EXE
*/

typedef enum
{
  KEY_SHIFT = 0,
  KEY_BACK = 1,
  KEY_MODE = 2,
  KEY_UP = 3,
  KEY_OK = 4,

  KEY_LPAREN = 5,
  KEY_RPAREN = 6,
  KEY_LEFT = 7,
  KEY_DOWN = 8,
  KEY_RIGHT = 9,

  KEY_7 = 10,
  KEY_8 = 11,
  KEY_9 = 12,
  KEY_DEL = 13,
  KEY_AC = 14,

  KEY_4 = 15,
  KEY_5 = 16,
  KEY_6 = 17,
  KEY_MUL = 18,
  KEY_DIV = 19,

  KEY_1 = 20,
  KEY_2 = 21,
  KEY_3 = 22,
  KEY_ADD = 23,
  KEY_SUB = 24,

  KEY_0 = 25,
  KEY_DOT = 26,
  KEY_EXP = 27, 
  KEY_FMT = 28,
  KEY_EXE = 29
} key_id_t;

#define KEY_COUNT 30U

/* 未按 Shift 时插入的单个字符。NULL表示这是功能键。 */
extern const char key_primary[KEY_COUNT];

// 按下 Shift 后的文本；NULL 表示该第二功能未使用。
// 计算器引擎解释时采用单个字母，s=sin, c=cos, t=tan, l=log10, n=ln ,q=sqrt
extern const char *const key_shifted_text[KEY_COUNT];

#endif
