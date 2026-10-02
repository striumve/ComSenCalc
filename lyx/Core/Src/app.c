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
 * 静态存储。FreeRTOS 堆只有 1024 字节（见 FreeRTOSConfig.h），所以所有队列
 * 都用静态分配 —— 和线程的做法保持一致。
 *
 * 静态分配时 osMessageQueueNew() 需要同时提供控制块（cb_mem/cb_size）和
 * 存储区（mq_mem/mq_size）；只给存储区会返回 NULL，下一次调用就会撞上
 * configASSERT 而卡死。
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

/* touch_model_t 约 900 字节，绝不能放在任务栈上。 */
static touch_filter_t s_touch_filter;
static touch_model_t s_touch_model;

/* 上一次的运算结果，作为引擎的 answer 参数传入，这样第二功能的 FMT 键
 * （插入 'A'）就能复用它。 */
static calc_complex_t s_last_answer;

/* 控制器状态。 */
static char s_expr[APP_EXPR_MAX + 1U];
static uint8_t s_len;
static uint8_t s_cursor;
static uint8_t s_shift;
static uint8_t s_allow_complex;
static calc_angle_unit_t s_angle;

/* ---------------------------------------------------------------------------
 * 小的字符串 / 数字辅助函数。链接时用了 --specs=nano.specs，printf 不支持
 * 浮点，所以数字全部手工格式化。
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
        append_str(dst, size, &pos, "-");
        value = -value;
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

/* 把屏幕上显示的完整名字翻译成引擎认识的表达式写法。
 * 引擎只认单字母函数名（s=sin、c=cos、t=tan、l=log10、n=ln、q=sqrt），
 * 所以 "sin(" 要变成 "s("，括号保留：
 *     "sin(30)" -> "s(30)"，引擎读作 sin(30)。 */
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

/* ---------------------------------------------------------------------------
 * 显示内容的发布
 * ------------------------------------------------------------------------ */
static void publish_status(void)
{
    display_msg_t msg;
    uint8_t start;
    uint8_t i;

    memset(&msg, 0, sizeof(msg));

    /* 让光标始终落在 16 列的窗口内。 */
    start = (s_cursor > (APP_LCD_COLS - 1U)) ? (uint8_t)(s_cursor - (APP_LCD_COLS - 1U)) : 0U;

    for (i = 0U; i < APP_LCD_COLS; i++)
    {
        uint8_t index = (uint8_t)(start + i);
        msg.line1[i] = (index < s_len) ? s_expr[index] : ' ';
    }
    msg.line1[APP_LCD_COLS] = '\0';

    snprintf(msg.line2, sizeof(msg.line2), "Calc Ready%s%s",
             (s_shift != 0U) ? " S" : "",
             (s_allow_complex != 0U) ? " C" : "");

    msg.cursor_enabled = 1U;
    msg.cursor_row = 0U;
    msg.cursor_column = (uint8_t)(s_cursor - start);

    (void)osMessageQueuePut(s_display_queue, &msg, 0U, 0U);
}

static void publish_result(const char *result_text)
{
    display_msg_t msg;
    uint8_t start;
    uint8_t i;

    memset(&msg, 0, sizeof(msg));

    start = (s_cursor > (APP_LCD_COLS - 1U)) ? (uint8_t)(s_cursor - (APP_LCD_COLS - 1U)) : 0U;
    for (i = 0U; i < APP_LCD_COLS; i++)
    {
        uint8_t index = (uint8_t)(start + i);
        msg.line1[i] = (index < s_len) ? s_expr[index] : ' ';
    }
    msg.line1[APP_LCD_COLS] = '\0';

    /* 答案在 16 列里右对齐。 */
    {
        uint8_t length = (uint8_t)strlen(result_text);
        uint8_t pad = 0U;

        if (length > APP_LCD_COLS)
        {
            /* 比屏幕还宽：保留前面的字符。 */
            length = APP_LCD_COLS;
        }
        else
        {
            pad = (uint8_t)(APP_LCD_COLS - length);
        }

        for (i = 0U; i < pad; i++)
        {
            msg.line2[i] = ' ';
        }
        for (i = 0U; i < length; i++)
        {
            msg.line2[pad + i] = result_text[i];
        }
        msg.line2[APP_LCD_COLS] = '\0';
    }

    msg.cursor_enabled = 0U;

    (void)osMessageQueuePut(s_display_queue, &msg, 0U, 0U);
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
    expr_clear();
}

void app_heartbeat_task(void)
{
    for (;;)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        osDelay(HEARTBEAT_PERIOD_MS);
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

        if ((request.allow_complex != 0U) && (fabsf(result.imag) > 1.0e-6f))
        {
            uint8_t pos = 0U;

            text[0] = '\0';
            format_number(text, APP_TEXT_MAX, result.real);
            pos = (uint8_t)strlen(text);
            if ((pos < (APP_TEXT_MAX - 1U)) && (result.imag >= 0.0f))
            {
                append_str(text, APP_TEXT_MAX, &pos, "+");
            }
            format_number(text + pos, (uint8_t)(APP_TEXT_MAX - pos), result.imag);
            pos = (uint8_t)strlen(text);
            append_str(text, APP_TEXT_MAX, &pos, "i");
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
