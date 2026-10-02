#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>

/* 物理按键编号，就是 TTP229 那一对芯片报出来的编号。
 * 面板是 6 行 x 5 列，编号按行优先：
 * index = (行 - 1) * 5 + (列 - 1)。
 *
 *   第 1 行:  Shift   Back    Mode    上      OK
 *   第 2 行:  (       )       左      下      右
 *   第 3 行:  7       8       9       DEL     AC
 *   第 4 行:  4       5       6       *       /
 *   第 5 行:  1       2       3       +       -
 *   第 6 行:  0       .       10^     FMT     EXE
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
  KEY_EXP = 27, /* "10^" 键，插入 'E' 科学计数法 */
  KEY_FMT = 28,
  KEY_EXE = 29
} key_id_t;

#define KEY_COUNT 30U

/* 未按 Shift 时插入的单个字符。值为 NUL 表示这是由控制器处理的
 * 功能键。 */
extern const char key_primary[KEY_COUNT];

/* 按下 Shift 后再按键时插入的文本；NULL 表示该第二功能未使用。
 *
 * 这些是给人看的功能名，不是引擎的语法。计算器引擎解析的是单字母函数名
 * （s=sin、c=cos、t=tan、l=log10、n=ln、q=sqrt），所以 app.c 在求值之前
 * 会把这里的字符串翻译成那种形式。
 */
extern const char *const key_shifted_text[KEY_COUNT];

#endif
