/* 计算器引擎 表达式解析&求值 */

#include "calculator_engine.h"

#include <math.h>

#define CALC_PI 3.14159265358979f
#define CALC_E 2.71828182845905f
#define CALC_LN10 2.30258509299405f

/* 括号or函数的最大嵌套层数 */
#define CALC_MAX_DEPTH 12U

/* ---------------------------------------------------------------------------
 * 复数运算
 * ------------------------------------------------------------------------ */

static calc_complex_t cx(float real, float imag)
{
    calc_complex_t v;

    v.real = (real == 0.0f) ? 0.0f : real;
    v.imag = (imag == 0.0f) ? 0.0f : imag;
    return v;
}

static calc_complex_t cx_add(calc_complex_t a, calc_complex_t b)
{
    return cx(a.real + b.real, a.imag + b.imag);
}

static calc_complex_t cx_sub(calc_complex_t a, calc_complex_t b)
{
    return cx(a.real - b.real, a.imag - b.imag);
}

static calc_complex_t cx_neg(calc_complex_t a)
{
    return cx(-a.real, -a.imag);
}

static calc_complex_t cx_mul(calc_complex_t a, calc_complex_t b)
{
    return cx((a.real * b.real) - (a.imag * b.imag),
              (a.real * b.imag) + (a.imag * b.real));
}

static calc_complex_t cx_div(calc_complex_t a, calc_complex_t b,
                             calc_status_t *status)
{
    float d = (b.real * b.real) + (b.imag * b.imag);

    if (d == 0.0f)
    {
        *status = CALC_DIV_ZERO;
        return cx(0.0f, 0.0f);
    }
    return cx(((a.real * b.real) + (a.imag * b.imag)) / d,
              ((a.imag * b.real) - (a.real * b.imag)) / d);
}

/* 双曲函数 */
static float calc_sinh(float x)
{
    float e = expf(x);

    return (e - (1.0f / e)) * 0.5f;
}

static float calc_cosh(float x)
{
    float e = expf(x);

    return (e + (1.0f / e)) * 0.5f;
}

static calc_complex_t cx_exp(calc_complex_t a)
{
    float m = expf(a.real);

    return cx(m * cosf(a.imag), m * sinf(a.imag));
}

static calc_complex_t cx_log(calc_complex_t a, calc_status_t *status)
{
    if ((a.real == 0.0f) && (a.imag == 0.0f))
    {
        *status = CALC_DOMAIN;
        return cx(0.0f, 0.0f);
    }
    return cx(logf(hypotf(a.real, a.imag)), atan2f(a.imag, a.real));
}

/* 主值平方根：sqrt(z) = sqrt((|z|+a)/2) + i*sign(b)*sqrt((|z|-a)/2) */
static calc_complex_t cx_sqrt(calc_complex_t a)
{
    float m = hypotf(a.real, a.imag);
    float re = sqrtf((m + a.real) * 0.5f);
    float im = sqrtf((m - a.real) * 0.5f);

    if (a.imag < 0.0f)
    {
        im = -im;
    }
    return cx(re, im);
}

static calc_complex_t cx_sin(calc_complex_t a)
{
    return cx(sinf(a.real) * calc_cosh(a.imag),
              cosf(a.real) * calc_sinh(a.imag));
}

static calc_complex_t cx_cos(calc_complex_t a)
{
    return cx(cosf(a.real) * calc_cosh(a.imag),
              -sinf(a.real) * calc_sinh(a.imag));
}

static calc_complex_t cx_pow(calc_complex_t a, calc_complex_t b,
                             calc_status_t *status)
{
    /* 底数是正实数时用powf */
    if ((a.imag == 0.0f) && (b.imag == 0.0f) && (a.real > 0.0f))
    {
        return cx(powf(a.real, b.real), 0.0f);
    }

    /* 底数是负实数、指数是整数时，符号单独处理 */
    if ((a.imag == 0.0f) && (b.imag == 0.0f) && (a.real < 0.0f) &&
        (fabsf(b.real) < 1.0e9f))
    {
        long e = (long)b.real;

        if ((float)e == b.real)
        {
            float m = powf(-a.real, b.real);

            if ((e % 2) != 0)
            {
                m = -m;
            }
            return cx(m, 0.0f);
        }
    }

    if ((a.real == 0.0f) && (a.imag == 0.0f))
    {
        if (b.real > 0.0f)
        {
            return cx(0.0f, 0.0f);
        }
        *status = CALC_DIV_ZERO;
        return cx(0.0f, 0.0f);
    }

    /* 一般情形：z^w = exp(w * ln z) */
    {
        calc_complex_t l = cx_log(a, status);

        if (*status != CALC_OK)
        {
            return cx(0.0f, 0.0f);
        }
        return cx_exp(cx_mul(b, l));
    }
}

/* ---------------------------------------------------------------------------
 * 解析器状态
 * ------------------------------------------------------------------------ */
typedef struct
{
    const char *text;
    uint16_t pos;
    uint16_t depth;
    calc_angle_unit_t angle;
    uint8_t allow_complex;
    calc_complex_t answer;
    calc_status_t status;
} calc_ctx_t;

static char calc_peek(const calc_ctx_t *ctx)
{
    return ctx->text[ctx->pos];
}

static void calc_skip_spaces(calc_ctx_t *ctx)
{
    while (calc_peek(ctx) == ' ')
    {
        ctx->pos++;
    }
}

static calc_complex_t parse_expression(calc_ctx_t *ctx);
static calc_complex_t parse_unary(calc_ctx_t *ctx);
static calc_complex_t parse_primary(calc_ctx_t *ctx);

/* ---------------------------------------------------------------------------
 * 函数应用
 * ------------------------------------------------------------------------ */
static calc_complex_t to_radians(calc_complex_t a, calc_angle_unit_t unit)
{
    /* 只有实部参与角度换算 */
    if (unit == CALC_ANGLE_DEG)
    {
        a.real = a.real * CALC_PI / 180.0f;
    }
    return a;
}

static calc_complex_t apply_function(calc_ctx_t *ctx, char fn,
                                     calc_complex_t arg)
{
    switch (fn)
    {
    case 's':
        return cx_sin(to_radians(arg, ctx->angle));

    case 'c':
        return cx_cos(to_radians(arg, ctx->angle));

    case 't':
    {
        calc_complex_t s = cx_sin(to_radians(arg, ctx->angle));
        calc_complex_t c = cx_cos(to_radians(arg, ctx->angle));

        return cx_div(s, c, &ctx->status);
    }

    case 'n': /* ln */
        if ((ctx->allow_complex == 0U) && (arg.imag == 0.0f) && (arg.real <= 0.0f))
        {
            ctx->status = CALC_DOMAIN;
            return cx(0.0f, 0.0f);
        }
        return cx_log(arg, &ctx->status);

    case 'l': /* lg */
        if ((ctx->allow_complex == 0U) && (arg.imag == 0.0f) && (arg.real <= 0.0f))
        {
            ctx->status = CALC_DOMAIN;
            return cx(0.0f, 0.0f);
        }
        return cx_div(cx_log(arg, &ctx->status), cx(CALC_LN10, 0.0f),
                      &ctx->status);

    case 'q': /* sqrt */
        if ((ctx->allow_complex == 0U) && (arg.imag == 0.0f) && (arg.real < 0.0f))
        {
            ctx->status = CALC_DOMAIN;
            return cx(0.0f, 0.0f);
        }
        return cx_sqrt(arg);

    default:
        ctx->status = CALC_SYNTAX;
        return cx(0.0f, 0.0f);
    }
}

/* ---------------------------------------------------------------------------
 * 数字处理：整数部分.小数部分E指数
 * ------------------------------------------------------------------------ */
static calc_complex_t parse_number(calc_ctx_t *ctx)
{
    float mantissa = 0.0f;
    float scale = 1.0f;
    int32_t exponent = 0;
    int32_t sign = 1;
    char c;

    while (((c = calc_peek(ctx)) >= '0') && (c <= '9'))
    {
        mantissa = (mantissa * 10.0f) + (float)(c - '0');
        ctx->pos++;
    }

    if (calc_peek(ctx) == '.')
    {
        ctx->pos++;
        while (((c = calc_peek(ctx)) >= '0') && (c <= '9'))
        {
            mantissa = (mantissa * 10.0f) + (float)(c - '0');
            scale *= 10.0f;
            ctx->pos++;
        }
    }

    /* 指数 */
    if (calc_peek(ctx) == 'E')
    {
        ctx->pos++;
        if (calc_peek(ctx) == '+')
        {
            ctx->pos++;
        }
        else if (calc_peek(ctx) == '-')
        {
            sign = -1;
            ctx->pos++;
        }
        if ((calc_peek(ctx) < '0') || (calc_peek(ctx) > '9'))
        {
            ctx->status = CALC_SYNTAX;
            return cx(0.0f, 0.0f);
        }
        while (((c = calc_peek(ctx)) >= '0') && (c <= '9'))
        {
            if (exponent < 100)
            {
                exponent = (exponent * 10) + (c - '0');
            }
            ctx->pos++;
        }
        exponent *= sign;
    }

    mantissa = (mantissa / scale) * powf(10.0f, (float)exponent);
    return cx(mantissa, 0.0f);
}

/* ---------------------------------------------------------------------------
 * 基本表达式：数字、括号、常量、函数
 * ------------------------------------------------------------------------ */
static uint8_t is_function_letter(char c)
{
    return ((c == 's') || (c == 'c') || (c == 't') ||
            (c == 'l') || (c == 'n') || (c == 'q'))
               ? 1U
               : 0U;
}

static calc_complex_t parse_primary(calc_ctx_t *ctx)
{
    calc_complex_t value = cx(0.0f, 0.0f);
    char c;

    calc_skip_spaces(ctx);
    c = calc_peek(ctx);

    if (c == '(')
    {
        ctx->pos++;
        if (ctx->depth >= CALC_MAX_DEPTH)
        {
            ctx->status = CALC_SYNTAX;
            return cx(0.0f, 0.0f);
        }
        ctx->depth++;
        value = parse_expression(ctx);
        ctx->depth--;
        if (ctx->status != CALC_OK)
        {
            return cx(0.0f, 0.0f);
        }
        calc_skip_spaces(ctx);
        if (calc_peek(ctx) != ')')
        {
            ctx->status = CALC_SYNTAX;
            return cx(0.0f, 0.0f);
        }
        ctx->pos++;
        return value;
    }

    if (((c >= '0') && (c <= '9')) || (c == '.'))
    {
        return parse_number(ctx);
    }

    if (c == 'p')
    {
        ctx->pos++;
        return cx(CALC_PI, 0.0f);
    }

    if (c == 'e')
    {
        ctx->pos++;
        return cx(CALC_E, 0.0f);
    }

    if (c == 'i')
    {
        ctx->pos++;
        if (ctx->allow_complex == 0U)
        {
            ctx->status = CALC_SYNTAX;
            return cx(0.0f, 0.0f);
        }
        return cx(0.0f, 1.0f);
    }

    if (c == 'A')
    {
        ctx->pos++;
        return ctx->answer;
    }

    if (is_function_letter(c) != 0U)
    {
        char fn = c;

        ctx->pos++;
        calc_skip_spaces(ctx);
        if (ctx->depth >= CALC_MAX_DEPTH)
        {
            ctx->status = CALC_SYNTAX;
            return cx(0.0f, 0.0f);
        }
        ctx->depth++;
        value = parse_primary(ctx); 
        ctx->depth--;
        if (ctx->status != CALC_OK)
        {
            return cx(0.0f, 0.0f);
        }
        return apply_function(ctx, fn, value);
    }

    ctx->status = CALC_SYNTAX;
    return cx(0.0f, 0.0f);
}

/* ---------------------------------------------------------------------------
 * 优先级
 * ------------------------------------------------------------------------ */
static calc_complex_t parse_power(calc_ctx_t *ctx)
{
    calc_complex_t base = parse_primary(ctx);

    if (ctx->status != CALC_OK)
    {
        return cx(0.0f, 0.0f);
    }

    calc_skip_spaces(ctx);
    if (calc_peek(ctx) == '^')
    {
        calc_complex_t exponent;

        ctx->pos++;
        exponent = parse_unary(ctx);
        if (ctx->status != CALC_OK)
        {
            return cx(0.0f, 0.0f);
        }
        return cx_pow(base, exponent, &ctx->status);
    }
    return base;
}

static calc_complex_t parse_unary(calc_ctx_t *ctx)
{
    char c;

    calc_skip_spaces(ctx);
    c = calc_peek(ctx);

    if (c == '-')
    {
        ctx->pos++;
        return cx_neg(parse_unary(ctx));
    }
    if (c == '+')
    {
        ctx->pos++;
        return parse_unary(ctx);
    }
    return parse_power(ctx);
}

static calc_complex_t parse_term(calc_ctx_t *ctx)
{
    calc_complex_t value = parse_unary(ctx);

    for (;;)
    {
        char c;

        if (ctx->status != CALC_OK)
        {
            return cx(0.0f, 0.0f);
        }
        calc_skip_spaces(ctx);
        c = calc_peek(ctx);

        if (c == '*')
        {
            ctx->pos++;
            value = cx_mul(value, parse_unary(ctx));
        }
        else if (c == '/')
        {
            ctx->pos++;
            value = cx_div(value, parse_unary(ctx), &ctx->status);
        }
        else
        {
            return value;
        }
    }
}

static calc_complex_t parse_expression(calc_ctx_t *ctx)
{
    calc_complex_t value = parse_term(ctx);

    for (;;)
    {
        char c;

        if (ctx->status != CALC_OK)
        {
            return cx(0.0f, 0.0f);
        }
        calc_skip_spaces(ctx);
        c = calc_peek(ctx);

        if (c == '+')
        {
            ctx->pos++;
            value = cx_add(value, parse_term(ctx));
        }
        else if (c == '-')
        {
            ctx->pos++;
            value = cx_sub(value, parse_term(ctx));
        }
        else
        {
            return value;
        }
    }
}

/* ---------------------------------------------------------------------------
 * 对外接口
 * ------------------------------------------------------------------------ */
calc_status_t calculator_evaluate(const char *expression,
                                  calc_angle_unit_t angle_unit,
                                  uint8_t allow_complex,
                                  calc_complex_t answer,
                                  calc_complex_t *result)
{
    calc_ctx_t ctx;
    calc_complex_t value;

    if (result == NULL)
    {
        return CALC_SYNTAX;
    }
    result->real = 0.0f;
    result->imag = 0.0f;

    if (expression == NULL)
    {
        return CALC_SYNTAX;
    }

    ctx.text = expression;
    ctx.pos = 0U;
    ctx.depth = 0U;
    ctx.angle = angle_unit;
    ctx.allow_complex = allow_complex;
    ctx.answer = answer;
    ctx.status = CALC_OK;

    value = parse_expression(&ctx);
    if (ctx.status != CALC_OK)
    {
        return ctx.status;
    }

    /* 表达式必须被完整使用 */
    calc_skip_spaces(&ctx);
    if (ctx.text[ctx.pos] != '\0')
    {
        return CALC_SYNTAX;
    }

    /* 不允许复数时，结果必须是实数。 */
    if ((allow_complex == 0U) && (value.imag != 0.0f))
    {
        return CALC_DOMAIN;
    }

    *result = value;
    return CALC_OK;
}

calc_status_t calculator_solve_linear(float a, float b, float *x)
{
    if (x == NULL)
    {
        return CALC_SYNTAX;
    }
    *x = 0.0f;

    if (a == 0.0f)
    {
        return (b == 0.0f) ? CALC_OK : CALC_DOMAIN;
    }

    *x = -b / a;
    return CALC_OK;
}
