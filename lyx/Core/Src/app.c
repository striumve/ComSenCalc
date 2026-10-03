#include "app.h"

#include "main.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"

#include "keymap.h"
#include "keypad.h"
#include "screen.h"
#include "touch_filter.h"
#include "touch_model.h"
#include "calculator_engine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * 尺寸定义
 * ------------------------------------------------------------------------ */
#define APP_LCD_COLS 16U
/* 编辑缓冲区里存的是展开后的完整名字（"sin("、"log("），所以它比 LCD
 * 一次能显示的 16 列要宽。 */
#define APP_EXPR_MAX 47U
#define APP_TEXT_MAX 17U

#define KEY_QUEUE_LEN 8U
#define CALC_QUEUE_LEN 2U
#define DISPLAY_QUEUE_LEN 2U

#define KEY_TASK_PERIOD_MS 10U
#define HEARTBEAT_PERIOD_MS 500U

/* ---------------------------------------------------------------------------
 * 消息类型
 * ------------------------------------------------------------------------ */
typedef struct
{
    char expression[APP_EXPR_MAX + 1U];
    uint8_t angle_unit; /* calc_angle_unit_t */
    uint8_t allow_complex;
} calc_request_t;

typedef struct
{
    char line1[APP_TEXT_MAX];
    char line2[APP_TEXT_MAX];
    uint8_t cursor_enabled;
    uint8_t cursor_row;
    uint8_t cursor_column;
} display_msg_t;

/* ---------------------------------------------------------------------------
 * 静态存储
 * ------------------------------------------------------------------------ */
static uint8_t s_key_queue_storage[KEY_QUEUE_LEN] __attribute__((aligned(4)));
static StaticQueue_t s_key_queue_control;
static const osMessageQueueAttr_t s_key_queue_attr = {
    .name = "keyQueue",
    .cb_mem = &s_key_queue_control,
    .cb_size = sizeof(s_key_queue_control),
    .mq_mem = s_key_queue_storage,
    .mq_size = sizeof(s_key_queue_storage),
};

static calc_request_t s_calc_queue_storage[CALC_QUEUE_LEN] __attribute__((aligned(4)));
static StaticQueue_t s_calc_queue_control;
static const osMessageQueueAttr_t s_calc_queue_attr = {
    .name = "calcQueue",
    .cb_mem = &s_calc_queue_control,
    .cb_size = sizeof(s_calc_queue_control),
    .mq_mem = s_calc_queue_storage,
    .mq_size = sizeof(s_calc_queue_storage),
};

static display_msg_t s_display_queue_storage[DISPLAY_QUEUE_LEN] __attribute__((aligned(4)));
static StaticQueue_t s_display_queue_control;
static const osMessageQueueAttr_t s_display_queue_attr = {
    .name = "displayQueue",
    .cb_mem = &s_display_queue_control,
    .cb_size = sizeof(s_display_queue_control),
    .mq_mem = s_display_queue_storage,
    .mq_size = sizeof(s_display_queue_storage),
};

static osMessageQueueId_t s_key_queue;
static osMessageQueueId_t s_calc_queue;
static osMessageQueueId_t s_display_queue;

/* touch_model_t 约900字节，不能放在任务栈上。 */
static touch_filter_t s_touch_filter;
static touch_model_t s_touch_model;

/* 上一次的运算结果 */
static calc_complex_t s_last_answer;

/* 控制器状态 */
static char s_expr[APP_EXPR_MAX + 1U];
static uint8_t s_len;
static uint8_t s_cursor;
static uint8_t s_shift;
static uint8_t s_allow_complex;
static calc_angle_unit_t s_angle;

/* 解二次方程模式 */
static uint8_t s_solve_mode;               /* 0=计算器主页 1=解方程 */
static uint8_t s_solve_step;               /* 已录入的系数个数 0..2 */
static uint8_t s_solved;                   /* 1=正在显示结果 */
static float s_coeff[3];                   /* a、b、c */
static char s_root1[APP_TEXT_MAX];
static char s_root2[APP_TEXT_MAX];

/* ---------------------------------------------------------------------------
 * 小的字符串/数字辅助函数
 * ------------------------------------------------------------------------ */
static void append_str(char *dst, uint8_t size, uint8_t *pos, const char *src)
{
    while ((*src != '\0') && (*pos < (uint8_t)(size - 1U)))
    {
        dst[*pos] = *src;
        (*pos)++;
        src++;
    }
    dst[*pos] = '\0';
}

static void append_u32(char *dst, uint8_t size, uint8_t *pos, uint32_t value)
{
    char tmp[11];
    uint8_t n = 0U;

    if (value == 0U)
    {
        tmp[n] = '0';
        n++;
    }
    while ((value > 0U) && (n < 10U))
    {
        tmp[n] = (char)('0' + (value % 10U));
        n++;
        value /= 10U;
    }
    while ((n > 0U) && (*pos < (uint8_t)(size - 1U)))
    {
        n--;
        dst[*pos] = tmp[n];
        (*pos)++;
    }
    dst[*pos] = '\0';
}

/* 定点输出，最多 4 位小数，末尾多余的 0 去掉。 */
static void append_fixed(char *dst, uint8_t size, uint8_t *pos, float value)
{
    uint32_t scaled = (uint32_t)roundf(value * 10000.0f);
    uint32_t whole = scaled / 10000U;
    uint32_t frac = scaled % 10000U;
    char digits[4];
    uint8_t n = 0U;
    uint8_t i;

    append_u32(dst, size, pos, whole);

    for (i = 0U; i < 4U; i++)
    {
        digits[n] = (char)('0' + (frac % 10U));
        n++;
        frac /= 10U;
    }
    while ((n > 0U) && (digits[n - 1U] == '0'))
    {
        n--;
    }
    if (n > 0U)
    {
        append_str(dst, size, pos, ".");
        while ((n > 0U) && (*pos < (uint8_t)(size - 1U)))
        {
            n--;
            dst[*pos] = digits[n];
            (*pos)++;
        }
        dst[*pos] = '\0';
    }
}

static void format_number(char *dst, uint8_t size, float value)
{
    uint8_t pos = 0U;

    if (value != value)
    {
        append_str(dst, size, &pos, "NaN");
        return;
    }
    if (value > 1.0e38f)
    {
        append_str(dst, size, &pos, "Inf");
        return;
    }
    if (value < -1.0e38f)
    {
        append_str(dst, size, &pos, "-Inf");
        return;
    }
    if (value < 0.0f)
    {
        /* 小到 4 位小数都显示不出来时就当 0 */
        if (value > -0.00005f)
        {
            value = 0.0f;
        }
        else
        {
            append_str(dst, size, &pos, "-");
            value = -value;
        }
    }

    /* 整数值就不显示小数部分。 */
    if (value < 1.0e9f)
    {
        if (roundf(value) == value)
        {
            append_u32(dst, size, &pos, (uint32_t)value);
            return;
        }
        append_fixed(dst, size, &pos, value);
        return;
    }

    /* 特别大 / 特别小的数用科学计数法。 */
    {
        uint8_t exponent = 0U;

        while ((value >= 10.0f) && (exponent < 38U))
        {
            value /= 10.0f;
            exponent++;
        }
        while ((value < 1.0f) && (exponent > 0U))
        {
            value *= 10.0f;
            exponent--;
        }
        append_fixed(dst, size, &pos, value);
        append_str(dst, size, &pos, "e");
        append_u32(dst, size, &pos, (uint32_t)exponent);
    }
}

/* ---------------------------------------------------------------------------
 * 表达式编辑
 * ------------------------------------------------------------------------ */
static void expr_clear(void)
{
    s_len = 0U;
    s_cursor = 0U;
    s_expr[0] = '\0';
}

static void expr_insert(char c)
{
    uint8_t i;

    if (s_len >= APP_EXPR_MAX)
    {
        return;
    }
    for (i = s_len; i > s_cursor; i--)
    {
        s_expr[i] = s_expr[i - 1U];
    }
    s_expr[s_cursor] = c;
    s_len++;
    s_cursor++;
    s_expr[s_len] = '\0';
}

static void expr_insert_text(const char *text)
{
    while (*text != '\0')
    {
        expr_insert(*text);
        text++;
    }
}

static void expr_backspace(void)
{
    uint8_t i;

    if (s_cursor == 0U)
    {
        return;
    }
    for (i = (uint8_t)(s_cursor - 1U); i < (uint8_t)(s_len - 1U); i++)
    {
        s_expr[i] = s_expr[i + 1U];
    }
    s_len--;
    s_cursor--;
    s_expr[s_len] = '\0';
}

/* s=sin、c=cos、t=tan、l=log10、n=ln、q=sqrt */
static void display_to_engine(const char *display, char *engine, uint8_t size)
{
    uint8_t in = 0U;
    uint8_t out = 0U;

    while ((display[in] != '\0') && (out < (uint8_t)(size - 1U)))
    {
        if (strncmp(&display[in], "sqrt", 4) == 0)
        {
            engine[out] = 'q';
            out++;
            in += 4U;
        }
        else if (strncmp(&display[in], "sin", 3) == 0)
        {
            engine[out] = 's';
            out++;
            in += 3U;
        }
        else if (strncmp(&display[in], "cos", 3) == 0)
        {
            engine[out] = 'c';
            out++;
            in += 3U;
        }
        else if (strncmp(&display[in], "tan", 3) == 0)
        {
            engine[out] = 't';
            out++;
            in += 3U;
        }
        else if (strncmp(&display[in], "log", 3) == 0)
        {
            engine[out] = 'l';
            out++;
            in += 3U;
        }
        else if (strncmp(&display[in], "ln", 2) == 0)
        {
            engine[out] = 'n';
            out++;
            in += 2U;
        }
        else
        {
            engine[out] = display[in];
            out++;
            in++;
        }
    }
    engine[out] = '\0';
}

/* 把结果值格式化成文本：虚部可忽略时只显示实数，否则显示 a+bi。 */
static void format_complex(char *dst, uint8_t size, calc_complex_t value)
{
    if (fabsf(value.imag) > 1.0e-6f)
    {
        uint8_t pos = 0U;

        dst[0] = '\0';
        format_number(dst, size, value.real);
        pos = (uint8_t)strlen(dst);
        if ((pos < (uint8_t)(size - 1U)) && (value.imag >= 0.0f))
        {
            append_str(dst, size, &pos, "+");
        }
        format_number(dst + pos, (uint8_t)(size - pos), value.imag);
        pos = (uint8_t)strlen(dst);
        append_str(dst, size, &pos, "i");
        return;
    }
    format_number(dst, size, value.real);
}

/* 把编辑缓冲区渲染成 LCD 实际显示的字符序列。
 *
 * 部分符号换成 CGRAM 自定义字形。"sqrt" 是 4 个字符、渲染成 √ 只有 1 个，
 * 所以渲染后的长度可能比缓冲区短，光标位置必须跟着换算，否则光标会
 * 指到错误的地方。返回渲染后的长度，光标下标写入 *cursor_out。 */
static uint8_t render_expression(char *dst, uint8_t dst_size, uint8_t *cursor_out)
{
    uint8_t in = 0U;
    uint8_t out = 0U;

    *cursor_out = 0U;

    while ((in < s_len) && (out < (uint8_t)(dst_size - 1U)))
    {
        if (in == s_cursor)
        {
            *cursor_out = out;
        }

        if (((uint8_t)(s_len - in) >= 4U) &&
            (strncmp(&s_expr[in], "sqrt", 4) == 0))
        {
            dst[out] = (char)LCD_CHAR_SQRT;
            out++;
            in += 4U;
            continue;
        }

        switch (s_expr[in])
        {
            case '*': dst[out] = (char)LCD_CHAR_MUL; break;
            case '/': dst[out] = (char)LCD_CHAR_DIV; break;
            case 'p': dst[out] = (char)LCD_CHAR_PI; break;
            /* 乘方直接显示 ASCII 的 '^'，不用自定义字形 */
            default:  dst[out] = s_expr[in]; break;
        }
        out++;
        in++;
    }

    /* 光标在末尾时不会命中循环里的判断，这里补上。 */
    if (in == s_cursor)
    {
        *cursor_out = out;
    }

    dst[out] = '\0';
    return out;
}

/* 把表达式渲染并开窗到 LCD 第 1 行，返回光标所在的列。
 *
 * first_col 是表达式起始列，前面的列留给调用者自己填（解方程模式用它放
 * "a="/"b="/"c=" 提示）。这些列不会被本函数覆盖。 */
static uint8_t build_expression_line(char *line, uint8_t first_col)
{
    char rendered[APP_EXPR_MAX + 1U];
    uint8_t rendered_len;
    uint8_t cursor;
    uint8_t cols;
    uint8_t start;
    uint8_t i;

    rendered_len = render_expression(rendered, (uint8_t)sizeof(rendered), &cursor);

    cols = (uint8_t)(APP_LCD_COLS - first_col);

    /* 窗口滚动：让光标落在剩余列之内 */
    start = (cursor > (uint8_t)(cols - 1U)) ? (uint8_t)(cursor - (cols - 1U)) : 0U;

    for (i = first_col; i < APP_LCD_COLS; i++)
    {
        line[i] = ' ';
    }
    for (i = 0U; i < cols; i++)
    {
        uint8_t index = (uint8_t)(start + i);
        if (index < rendered_len)
        {
            line[(uint8_t)(first_col + i)] = rendered[index];
        }
    }
    line[APP_LCD_COLS] = '\0';

    return (uint8_t)(first_col + cursor - start);
}

/* 把文本右对齐铺进 16 列；比屏幕宽时保留前面的字符。 */
static void align_right(char *dst, const char *text)
{
    uint8_t length = (uint8_t)strlen(text);
    uint8_t pad = 0U;
    uint8_t i;

    if (length > APP_LCD_COLS)
    {
        length = APP_LCD_COLS;
    }
    else
    {
        pad = (uint8_t)(APP_LCD_COLS - length);
    }

    for (i = 0U; i < pad; i++)
    {
        dst[i] = ' ';
    }
    for (i = 0U; i < length; i++)
    {
        dst[pad + i] = text[i];
    }
    dst[APP_LCD_COLS] = '\0';
}

/* ---------------------------------------------------------------------------
 * 解二次方程
 * ------------------------------------------------------------------------ */

/* 用当前的输入求出一个系数。输入走完整表达式求值，返回 0 表示失败。 */
static uint8_t solve_read_coefficient(float *out)
{
    char engine[APP_EXPR_MAX + 1U];
    calc_complex_t result;

    if (s_len == 0U)
    {
        return 0U; /* 空输入，EXE 不响应 */
    }

    display_to_engine(s_expr, engine, (uint8_t)sizeof(engine));
    result.real = 0.0f;
    result.imag = 0.0f;

    if (calculator_evaluate(engine, s_angle, 0U, s_last_answer, &result) != CALC_OK)
    {
        return 0U;
    }
    if (fabsf(result.imag) > 1.0e-6f)
    {
        return 0U; /* 系数必须是实数 */
    }

    *out = result.real;
    return 1U;
}

static void solve_quadratic(void)
{
    calc_complex_t r1;
    calc_complex_t r2;

    if (calculator_solve_quadratic(s_coeff[0], s_coeff[1], s_coeff[2],
                                   &r1, &r2) != CALC_OK)
    {
        /* 只有 a=0 且 b=0 且 c!=0 才会走到这里：无解。 */
        strncpy(s_root1, "no root", APP_TEXT_MAX - 1U);
        s_root1[APP_TEXT_MAX - 1U] = '\0';
        s_root2[0] = '\0';
        return;
    }

    format_complex(s_root1, APP_TEXT_MAX, r1);
    format_complex(s_root2, APP_TEXT_MAX, r2);
}

static void solve_reset(void)
{
    s_solve_step = 0U;
    s_solved = 0U;
    s_coeff[0] = 0.0f;
    s_coeff[1] = 0.0f;
    s_coeff[2] = 0.0f;
    s_root1[0] = '\0';
    s_root2[0] = '\0';
    expr_clear();
}

/* ---------------------------------------------------------------------------
 * 显示内容的发布
 * ------------------------------------------------------------------------ */
static void publish_status(void)
{
    display_msg_t msg;
    uint8_t cursor_column;

    memset(&msg, 0, sizeof(msg));

    cursor_column = build_expression_line(msg.line1, 0U);

    /* 第二行状态：先显示复数标志 C，再显示 Shift 指示（上箭头自定义字形）。 */
    {
        uint8_t pos = 0U;

        memcpy(msg.line2, "Calc Ready", 10U);
        pos = 10U;
        if (s_allow_complex != 0U)
        {
            msg.line2[pos] = ' ';
            msg.line2[pos + 1U] = 'C';
            pos += 2U;
        }
        if (s_shift != 0U)
        {
            msg.line2[pos] = ' ';
            msg.line2[pos + 1U] = (char)LCD_CHAR_UP;
            pos += 2U;
        }
        msg.line2[pos] = '\0';
    }

    msg.cursor_enabled = 1U;
    msg.cursor_row = 0U;
    msg.cursor_column = cursor_column;

    (void)osMessageQueuePut(s_display_queue, &msg, 0U, 0U);
}

static void publish_result(const char *result_text)
{
    display_msg_t msg;

    memset(&msg, 0, sizeof(msg));

    (void)build_expression_line(msg.line1, 0U);
    align_right(msg.line2, result_text); /* 答案右对齐 */
    msg.cursor_enabled = 0U;

    (void)osMessageQueuePut(s_display_queue, &msg, 0U, 0U);
}

/* 解方程模式的显示：输入阶段第一行是 "a="/"b="/"c=" 加正在输入的内容，
 * 第二行提示处于解方程模式；出结果时第一行 x1、第二行 x2。 */
static void publish_solve(void)
{
    display_msg_t msg;

    memset(&msg, 0, sizeof(msg));

    if (s_solved != 0U)
    {
        align_right(msg.line1, s_root1);
        align_right(msg.line2, s_root2);
        msg.cursor_enabled = 0U;
    }
    else
    {
        /* 提示当前在输入哪个系数：a= / b= / c= */
        msg.line1[0] = (char)('a' + s_solve_step);
        msg.line1[1] = '=';
        msg.cursor_column = build_expression_line(msg.line1, 2U);

        /* 第二行提示处于解方程模式 */
        memcpy(msg.line2, "Solve mode", 10U);
        msg.line2[10] = '\0';

        msg.cursor_enabled = 1U;
        msg.cursor_row = 0U;
    }

    (void)osMessageQueuePut(s_display_queue, &msg, 0U, 0U);
}

/* ---------------------------------------------------------------------------
 * USB 串口终端
 *
 * CDC_Receive_FS() 运行在 USB 中断上下文里，那里不能调用可能阻塞的
 * CMSIS-RTOS2 队列接口，所以用一个单生产者（中断）/ 单消费者（任务）的
 * 环形缓冲区把数据交给任务处理。
 *
 * 主页按 Shift + 0 进入，整屏变成 16x2 终端；再按一次退出。
 * ------------------------------------------------------------------------ */
#define USB_RX_RING_SIZE 128U
#define USB_POLL_PERIOD_MS 10U
#define USB_TERM_ROWS 2U

static volatile uint8_t s_usb_rx[USB_RX_RING_SIZE];
static volatile uint16_t s_usb_rx_head;
static volatile uint16_t s_usb_rx_tail;

static char s_usb_term[USB_TERM_ROWS][APP_LCD_COLS + 1U];
static uint8_t s_usb_row;
static uint8_t s_usb_col;
static uint8_t s_usb_mode;
static uint8_t s_usb_dirty;

/* 由 USB 中断调用，把收到的字节放进环形缓冲区。 */
void app_usb_rx_push(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint16_t head = s_usb_rx_head;

    for (i = 0U; i < len; i++)
    {
        uint16_t next = (uint16_t)((head + 1U) % USB_RX_RING_SIZE);

        if (next == s_usb_rx_tail)
        {
            break; /* 满了，多余的字节丢掉 */
        }
        s_usb_rx[head] = data[i];
        head = next;
    }
    s_usb_rx_head = head;
}

static uint8_t usb_rx_pop(uint8_t *ch)
{
    if (s_usb_rx_tail == s_usb_rx_head)
    {
        return 0U;
    }
    *ch = s_usb_rx[s_usb_rx_tail];
    s_usb_rx_tail = (uint16_t)((s_usb_rx_tail + 1U) % USB_RX_RING_SIZE);
    return 1U;
}

static void usb_term_clear(void)
{
    uint8_t row;
    uint8_t col;

    for (row = 0U; row < USB_TERM_ROWS; row++)
    {
        for (col = 0U; col < APP_LCD_COLS; col++)
        {
            s_usb_term[row][col] = ' ';
        }
        s_usb_term[row][APP_LCD_COLS] = '\0';
    }
    s_usb_row = 0U;
    s_usb_col = 0U;
}

/* 整屏上滚一行。 */
static void usb_term_scroll(void)
{
    uint8_t col;

    for (col = 0U; col < APP_LCD_COLS; col++)
    {
        s_usb_term[0][col] = s_usb_term[1][col];
        s_usb_term[1][col] = ' ';
    }
    s_usb_row = 1U;
    s_usb_col = 0U;
}

static void usb_term_newline(void)
{
    if (s_usb_row == 0U)
    {
        s_usb_row = 1U;
        s_usb_col = 0U;
    }
    else
    {
        usb_term_scroll();
    }
}

static void usb_term_putc(char ch)
{
    if ((uint8_t)ch < 0x20U)
    {
        return; /* 控制字符忽略 */
    }
    s_usb_term[s_usb_row][s_usb_col] = ch;
    s_usb_col++;
    if (s_usb_col >= APP_LCD_COLS)
    {
        if (s_usb_row == 0U)
        {
            s_usb_row = 1U;
            s_usb_col = 0U;
        }
        else
        {
            usb_term_scroll();
        }
    }
}

static void publish_usb_terminal(void)
{
    display_msg_t msg;

    memset(&msg, 0, sizeof(msg));
    memcpy(msg.line1, s_usb_term[0], APP_LCD_COLS + 1U);
    memcpy(msg.line2, s_usb_term[1], APP_LCD_COLS + 1U);

    msg.cursor_enabled = 1U;
    msg.cursor_row = s_usb_row;
    msg.cursor_column = (s_usb_col < APP_LCD_COLS) ? s_usb_col : (APP_LCD_COLS - 1U);

    (void)osMessageQueuePut(s_display_queue, &msg, 0U, 0U);
}

static void usb_terminal_toggle(void)
{
    if (s_usb_mode == 0U)
    {
        usb_term_clear();
        s_usb_mode = 1U;
        s_usb_dirty = 0U;
        publish_usb_terminal();
    }
    else
    {
        s_usb_mode = 0U;
    }
}

/* 把环形缓冲区里的字节喂给终端。由 defaultTask 周期调用。 */
static void usb_poll(void)
{
    uint8_t ch;

    while (usb_rx_pop(&ch) != 0U)
    {
        if (ch == '\n')
        {
            usb_term_newline();
        }
        else if (ch != '\r')
        {
            usb_term_putc((char)ch);
        }
        s_usb_dirty = 1U;
    }

    if ((s_usb_mode != 0U) && (s_usb_dirty != 0U))
    {
        s_usb_dirty = 0U;
        publish_usb_terminal();
    }
}

/* ---------------------------------------------------------------------------
 * 对外入口
 * ------------------------------------------------------------------------ */
void app_init(void)
{
    s_key_queue = osMessageQueueNew(KEY_QUEUE_LEN, sizeof(uint8_t), &s_key_queue_attr);
    s_calc_queue = osMessageQueueNew(CALC_QUEUE_LEN, sizeof(calc_request_t), &s_calc_queue_attr);
    s_display_queue = osMessageQueueNew(DISPLAY_QUEUE_LEN, sizeof(display_msg_t), &s_display_queue_attr);

    s_angle = CALC_ANGLE_DEG;
    s_allow_complex = 0U;
    s_shift = 0U;
    s_solve_mode = 0U; 
    s_usb_mode = 0U;
    s_usb_dirty = 0U;
    s_usb_rx_head = 0U;
    s_usb_rx_tail = 0U;
    usb_term_clear();
    solve_reset();
    expr_clear();
}

void app_heartbeat_task(void)
{
    uint16_t ticks = 0U;

    for (;;)
    {
        /* USB 串口接收必须在普通任务里处理（中断里只往环形缓冲区塞字节）。 */
        usb_poll();

        ticks++;
        if (ticks >= (HEARTBEAT_PERIOD_MS / USB_POLL_PERIOD_MS))
        {
            ticks = 0U;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

        osDelay(USB_POLL_PERIOD_MS);
    }
}

void app_key_task(void)
{
    uint32_t previous = 0U;

    keypad_init();
    touch_filter_init(&s_touch_filter);
    touch_model_load_default(&s_touch_model);

    for (;;)
    {
        uint32_t raw = touch_raw_read();
        uint32_t key;

        (void)touch_filter_update(&s_touch_filter, raw);
        key = touch_model_classify(&s_touch_model, s_touch_filter.stable_bitmap);

        /* 只在上升沿上报按键，这样按住不放不会把队列灌满。 */
        if ((key != 0U) && (previous == 0U))
        {
            uint8_t index = keypad_first_key(key);
            if (index < KEY_COUNT)
            {
                (void)osMessageQueuePut(s_key_queue, &index, 0U, 0U);
            }
        }
        previous = key;

        osDelay(KEY_TASK_PERIOD_MS);
    }
}

void app_controller_task(void)
{
    uint8_t key;

    publish_status();

    for (;;)
    {
        if (osMessageQueueGet(s_key_queue, &key, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        /* ---- USB 串口终端模式 ---- */
        if (s_usb_mode != 0U)
        {
            /* Shift + 0 退出，回到计算器主页。 */
            if ((key == KEY_0) && (s_shift != 0U))
            {
                s_shift = 0U;
                s_usb_mode = 0U;
                publish_status();
                continue;
            }

            /* 只认 Shift（用来凑出 Shift+0），其余按键忽略，免得在看不见的
             * 地方改动了表达式。 */
            if (key == KEY_SHIFT)
            {
                s_shift = (s_shift == 0U) ? 1U : 0U;
            }
            continue;
        }

        /* ---- 解二次方程模式 ---- */
        if (s_solve_mode != 0U)
        {
            /* Shift + . 随时退出，回到计算器主页。 */
            if ((key == KEY_DOT) && (s_shift != 0U))
            {
                s_shift = 0U;
                s_solve_mode = 0U;
                solve_reset();
                publish_status();
                continue;
            }

            if (key == KEY_SHIFT)
            {
                s_shift = (s_shift == 0U) ? 1U : 0U;
                continue;
            }

            /* 结果已经显示时，再按任何输入类按键就开始解下一个方程。 */
            if (s_solved != 0U)
            {
                if ((key == KEY_EXE) || (key == KEY_OK) || (key == KEY_DEL) ||
                    (key == KEY_BACK) || (key == KEY_AC) ||
                    (key == KEY_LEFT) || (key == KEY_RIGHT))
                {
                    continue; /* 忽略 */
                }
                solve_reset();
            }

            switch (key)
            {
                case KEY_EXE:
                case KEY_OK:
                {
                    float value = 0.0f;

                    if (solve_read_coefficient(&value) != 0U)
                    {
                        s_coeff[s_solve_step] = value;
                        s_solve_step++;
                        expr_clear();
                        if (s_solve_step >= 3U)
                        {
                            solve_quadratic();
                            s_solved = 1U;
                        }
                    }
                    break;
                }

                case KEY_AC:
                    expr_clear();
                    break;

                case KEY_BACK:
                case KEY_DEL:
                    expr_backspace();
                    break;

                case KEY_LEFT:
                    if (s_cursor > 0U)
                    {
                        s_cursor--;
                    }
                    break;

                case KEY_RIGHT:
                    if (s_cursor < s_len)
                    {
                        s_cursor++;
                    }
                    break;

                default:
                    if (key < KEY_COUNT)
                    {
                        if ((s_shift != 0U) && (key_shifted_text[key] != NULL))
                        {
                            expr_insert_text(key_shifted_text[key]);
                        }
                        else if (key_primary[key] != '\0')
                        {
                            expr_insert(key_primary[key]);
                        }
                    }
                    break;
            }

            s_shift = 0U;
            publish_solve();
            continue;
        }

        /* ---- 计算器主页 ---- */
        switch (key)
        {
            case KEY_SHIFT:
                s_shift = (s_shift == 0U) ? 1U : 0U;
                break;

            case KEY_AC:
                expr_clear();
                s_shift = 0U;
                break;

            case KEY_BACK:
            case KEY_DEL:
                expr_backspace();
                s_shift = 0U;
                break;

            case KEY_LEFT:
                if (s_cursor > 0U)
                {
                    s_cursor--;
                }
                s_shift = 0U;
                break;

            case KEY_RIGHT:
                if (s_cursor < s_len)
                {
                    s_cursor++;
                }
                s_shift = 0U;
                break;

            case KEY_FMT:
                s_angle = (s_angle == CALC_ANGLE_DEG) ? CALC_ANGLE_RAD : CALC_ANGLE_DEG;
                s_shift = 0U;
                break;

            case KEY_MODE:
                s_allow_complex = (s_allow_complex == 0U) ? 1U : 0U;
                s_shift = 0U;
                break;

            case KEY_EXE:
            case KEY_OK:
                if (s_len > 0U)
                {
                    calc_request_t request;

                    memset(&request, 0, sizeof(request));
                    display_to_engine(s_expr, request.expression,
                                      (uint8_t)sizeof(request.expression));
                    request.angle_unit = (uint8_t)s_angle;
                    request.allow_complex = s_allow_complex;
                    (void)osMessageQueuePut(s_calc_queue, &request, 0U, 0U);
                }
                s_shift = 0U;
                break;

            /* 小数点键：Shift + . 进入解二次方程模式。 */
            case KEY_DOT:
                if (s_shift != 0U)
                {
                    s_shift = 0U;
                    s_solve_mode = 1U;
                    solve_reset();
                    publish_solve();
                    continue; /* 已经发过显示消息，跳过末尾的 publish */
                }
                expr_insert(key_primary[key]);
                s_shift = 0U;
                break;

            /* 数字 0：Shift + 0 进入 USB 串口终端模式。 */
            case KEY_0:
                if (s_shift != 0U)
                {
                    s_shift = 0U;
                    usb_terminal_toggle();
                    continue; /* 已经发过显示消息，跳过末尾的 publish */
                }
                expr_insert(key_primary[key]);
                s_shift = 0U;
                break;

            default:
                if (key < KEY_COUNT)
                {
                    if (s_shift != 0U)
                    {
                        if (key_shifted_text[key] != NULL)
                        {
                            expr_insert_text(key_shifted_text[key]);
                        }
                    }
                    else if (key_primary[key] != '\0')
                    {
                        expr_insert(key_primary[key]);
                    }
                    s_shift = 0U;
                }
                break;
        }

        publish_status();
    }
}

void app_compute_task(void)
{
    calc_request_t request;

    for (;;)
    {
        calc_complex_t answer;
        calc_complex_t result;
        calc_status_t status;
        char text[APP_TEXT_MAX];

        if (osMessageQueueGet(s_calc_queue, &request, NULL, osWaitForever) != osOK)
        {
            continue;
        }

        answer.real = s_last_answer.real;
        answer.imag = s_last_answer.imag;
        result.real = 0.0f;
        result.imag = 0.0f;

        status = calculator_evaluate(request.expression,
                                     (calc_angle_unit_t)request.angle_unit,
                                     request.allow_complex,
                                     answer,
                                     &result);

        if (status != CALC_OK)
        {
            const char *message = "error";
            if (status == CALC_SYNTAX)
            {
                message = "syntax err";
            }
            else if (status == CALC_DOMAIN)
            {
                message = "domain err";
            }
            else if (status == CALC_DIV_ZERO)
            {
                message = "div by 0";
            }
            publish_result(message);
            continue;
        }

        /* 复数模式下虚部有意义；否则只显示实部。 */
        if (request.allow_complex != 0U)
        {
            format_complex(text, APP_TEXT_MAX, result);
        }
        else
        {
            format_number(text, APP_TEXT_MAX, result.real);
        }

        s_last_answer = result;
        publish_result(text);
    }
}

void app_lcd_task(void)
{
    display_msg_t msg;

    screen_init();
    screen_write_lines("ComSen Calc", "Calc Ready");

    for (;;)
    {
        if (osMessageQueueGet(s_display_queue, &msg, NULL, osWaitForever) == osOK)
        {
            screen_write_frame(msg.line1, msg.line2,
                               msg.cursor_enabled, msg.cursor_row,
                               msg.cursor_column);
        }
    }
}
