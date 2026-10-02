#include "keymap.h"

/* 从参考实现的 .rodata.primary.0（libcalculator_app.a）解出来的。
 * 下标就是 keymap.h 里的物理按键编号。 */
const char key_primary[KEY_COUNT] = {
    0,   0,   0,   0,   0,          /* Shift Back Mode 上   OK      */
    '(', ')', 0,   0,   0,          /* (  )  左  下  右             */
    '7', '8', '9', 0,   0,          /* 7  8  9  DEL AC              */
    '4', '5', '6', '*', '/',
    '1', '2', '3', '+', '-',
    '0', '.', 'E', 0,   0           /* 0  .  10^ FMT EXE            */
};

/* 第二功能，写成完整名字方便显示。
 *
 * 引擎只认单字母 s/c/t/l/n/q，所以 app.c 在求值前会把这些名字翻译回去。
 * 末尾的 '(' 是文本的一部分，因为函数作用在括号里的参数上。
 *
 *   p = π           e = 自然常数 e      i = 虚数单位
 *   A = 上次答案                        ^ = 乘方
 *   第 11 号键的第二功能（参考表里是 'd'）未使用：引擎没有定义 d 函数。
 */
static const char s_pi[] = "p";
static const char s_i[] = "i";
static const char s_e[] = "e";
static const char s_log[] = "log(";
static const char s_ln[] = "ln(";
static const char s_pow[] = "^";
static const char s_sqrt[] = "sqrt(";
static const char s_sin[] = "sin(";
static const char s_cos[] = "cos(";
static const char s_tan[] = "tan(";
static const char s_ans[] = "A";

const char *const key_shifted_text[KEY_COUNT] = {
    0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,
    s_pi,  0,     s_i,   0,     0,      /* 7 8 9            */
    s_e,   s_log, s_ln,  s_pow, s_sqrt, /* 4 5 6  *  /      */
    s_sin, s_cos, s_tan, 0,     0,      /* 1 2 3  +  -      */
    0,     0,     0,     s_ans, 0       /* 0 . 10^ FMT EXE  */
};
