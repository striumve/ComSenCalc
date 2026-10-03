/* AI自用，用于测试代码是否可行 */

#include "../Core/Inc/calculator_engine.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

static void expect(const char *expr, calc_angle_unit_t unit,
                   uint8_t complex_ok, calc_complex_t answer,
                   calc_status_t want_status, float want_real, float want_imag)
{
  calc_complex_t out;
  calc_status_t st;
  int ok;

  out.real = 0.0f;
  out.imag = 0.0f;
  st = calculator_evaluate(expr, unit, complex_ok, answer, &out);

  ok = (st == want_status);
  if (ok && (st == CALC_OK))
  {
    ok = (fabsf(out.real - want_real) < 1.0e-4f) &&
         (fabsf(out.imag - want_imag) < 1.0e-4f);
  }

  if (ok)
  {
    g_pass++;
    printf("  ok   %-16s -> %.4f %+.4fi\n", expr, out.real, out.imag);
  }
  else
  {
    g_fail++;
    printf("  FAIL %-16s -> status=%d real=%.4f imag=%.4f (expected status=%d real=%.4f imag=%.4f)\n",
           expr, (int)st, out.real, out.imag, (int)want_status, want_real, want_imag);
  }
}

static void ok(const char *expr, float want)
{
  calc_complex_t z;

  z.real = 0.0f;
  z.imag = 0.0f;
  expect(expr, CALC_ANGLE_DEG, 0U, z, CALC_OK, want, 0.0f);
}

int main(void)
{
  calc_complex_t zero;

  zero.real = 0.0f;
  zero.imag = 0.0f;

  printf("--- 四则运算与优先级 ---\n");
  ok("1+2", 3.0f);
  ok("9-4", 5.0f);
  ok("3*4", 12.0f);
  ok("8/2", 4.0f);
  ok("2+3*4", 14.0f);
  ok("(2+3)*4", 20.0f);
  ok("1+2*3-4/2", 5.0f);
  ok("-5+3", -2.0f);
  ok("--5", 5.0f);

  printf("--- 乘方 ---\n");
  ok("2^10", 1024.0f);
  ok("-2^2", -4.0f);   /* 一元负号低于 ^ */
  ok("2^3^2", 512.0f); /* ^ 右结合 */
  ok("(-2)^3", -8.0f);
  ok("2^-2", 0.25f);

  printf("--- 函数（单字母 + 一个 primary）---\n");
  ok("q9", 3.0f);
  ok("q(9)", 3.0f);
  ok("q9+1", 4.0f);    /* 不是 sqrt(10) */
  ok("q(9+7)", 4.0f);  /* 括号包住才是 sqrt(16) */
  ok("l100", 2.0f);    /* log10 */
  ok("n1", 0.0f);      /* ln(1) */
  ok("s30", 0.5f);     /* DEG 模式下 sin(30) */
  ok("s(30)", 0.5f);
  ok("c60", 0.5f);
  ok("t45", 1.0f);
  ok("s(90)", 1.0f);

  printf("--- 角度制切换 ---\n");
  expect("s(p/2)", CALC_ANGLE_RAD, 0U, zero, CALC_OK, 1.0f, 0.0f); /* sin(pi/2) */
  ok("s90", 1.0f);

  printf("--- 默认角度制是 DEG ---\n");
  /* app.c 的 app_init() 里 s_angle = CALC_ANGLE_DEG，所以 s1 = sin(1度)。
   * 角度制: sin(1°)   = 0.0174524
   * 弧度制: sin(1 rad) = 0.8414710
   * 两者差 48 倍，很容易看出来。 */
  ok("s1", 0.0174524f);
  expect("s1", CALC_ANGLE_RAD, 0U, zero, CALC_OK, 0.8414710f, 0.0f);
  ok("c1", 0.9998477f);
  ok("t1", 0.0174551f);

  printf("--- 非复数模式下，实表达式的虚部必须精确为 0 ---\n");
  /* ok() 用的就是 allow_complex=0，所以下面每个用例都在证明：
   * 纯实表达式不会被误判成 CALC_DOMAIN。
   * 这几个最容易出问题：
   *   c90   -> cos 的虚部 = -sin(pi/2)*sinh(0) = -1 * 0 = -0.0f
   *   s180  -> sin(pi) 在 float 下是 -8.7e-8，不是精确 0
   *   q(2)*q(2) -> 先开方再相乘，中间值带根号 */
  ok("c90", 0.0f);
  ok("s180", 0.0f);
  ok("q(2)*q(2)", 2.0f);

  printf("--- 常量与科学计数法 ---\n");
  ok("1E3", 1000.0f);
  ok("2.5E-3", 0.0025f);
  ok("1.5E2", 150.0f);
  ok("e", 2.71828f);
  ok("p", 3.14159f);
  ok("2*e", 5.43656f);

  printf("--- 上次答案 A ---\n");
  {
    calc_complex_t ans;

    ans.real = 5.0f;
    ans.imag = 0.0f;
    expect("A+1", CALC_ANGLE_DEG, 0U, ans, CALC_OK, 6.0f, 0.0f);
    expect("A*A", CALC_ANGLE_DEG, 0U, ans, CALC_OK, 25.0f, 0.0f);
  }

  printf("--- 函数参数只取一个 primary（与参考实现一致）---\n");
  /* 这是刻意的：函数吃掉一个 primary，所以括号必须显式写。
   * 如果不是这样，s(2)^2 会被解析成 sin(4) 而不是 sin(2)^2。 */
  {
    calc_complex_t z;

    z.real = 0.0f;
    z.imag = 0.0f;
    expect("s(2)^2", CALC_ANGLE_RAD, 0U, z, CALC_OK, 0.826822f, 0.0f);
  }
  expect("q-4", CALC_ANGLE_DEG, 1U, zero, CALC_SYNTAX, 0.0f, 0.0f);

  printf("--- 复数 ---\n");
  {
    calc_complex_t ans;

    ans.real = 0.0f;
    ans.imag = 0.0f;
    expect("i*i", CALC_ANGLE_DEG, 1U, ans, CALC_OK, -1.0f, 0.0f);
    expect("(1+i)*(1-i)", CALC_ANGLE_DEG, 1U, ans, CALC_OK, 2.0f, 0.0f);
    expect("q(-4)", CALC_ANGLE_DEG, 1U, ans, CALC_OK, 0.0f, 2.0f);
    expect("n(-1)", CALC_ANGLE_DEG, 1U, ans, CALC_OK, 0.0f, 3.14159f);
    expect("i", CALC_ANGLE_DEG, 1U, ans, CALC_OK, 0.0f, 1.0f);
  }

  printf("--- 错误处理 ---\n");
  expect("1/0", CALC_ANGLE_DEG, 0U, zero, CALC_DIV_ZERO, 0.0f, 0.0f);
  expect("q(-4)", CALC_ANGLE_DEG, 0U, zero, CALC_DOMAIN, 0.0f, 0.0f); /* 复数未开 */
  expect("n0", CALC_ANGLE_DEG, 0U, zero, CALC_DOMAIN, 0.0f, 0.0f);
  expect("l(-5)", CALC_ANGLE_DEG, 0U, zero, CALC_DOMAIN, 0.0f, 0.0f);
  expect("i", CALC_ANGLE_DEG, 0U, zero, CALC_SYNTAX, 0.0f, 0.0f);     /* 复数未开 */
  expect("1+", CALC_ANGLE_DEG, 0U, zero, CALC_SYNTAX, 0.0f, 0.0f);
  expect("(1+2", CALC_ANGLE_DEG, 0U, zero, CALC_SYNTAX, 0.0f, 0.0f);
  expect("1+2)", CALC_ANGLE_DEG, 0U, zero, CALC_SYNTAX, 0.0f, 0.0f);
  expect("", CALC_ANGLE_DEG, 0U, zero, CALC_SYNTAX, 0.0f, 0.0f);
  expect("*5", CALC_ANGLE_DEG, 0U, zero, CALC_SYNTAX, 0.0f, 0.0f);
  expect("1E", CALC_ANGLE_DEG, 0U, zero, CALC_SYNTAX, 0.0f, 0.0f);

  printf("--- 嵌套深度保护（不能崩）---\n");
  {
    calc_complex_t out;
    calc_status_t st;

    out.real = 0.0f;
    out.imag = 0.0f;
    st = calculator_evaluate("((((((((((((((((((1))))))))))))))))))",
                             CALC_ANGLE_DEG, 0U, zero, &out);
    printf("  深嵌套 -> status=%d（不崩即可）\n", (int)st);
    g_pass++;
  }

  printf("--- calculator_solve_linear ---\n");
  {
    float x = 0.0f;
    calc_status_t st;

    st = calculator_solve_linear(2.0f, -6.0f, &x); /* 2x-6=0 -> x=3 */
    printf("  2x-6=0 -> status=%d x=%.4f %s\n", (int)st, x,
           ((st == CALC_OK) && (fabsf(x - 3.0f) < 1.0e-5f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (fabsf(x - 3.0f) < 1.0e-5f)) { g_pass++; } else { g_fail++; }

    st = calculator_solve_linear(0.0f, 5.0f, &x); /* 无解 */
    printf("  0x+5=0 -> status=%d %s\n", (int)st, (st == CALC_DOMAIN) ? "ok" : "FAIL");
    if (st == CALC_DOMAIN) { g_pass++; } else { g_fail++; }
  }

  printf("--- calculator_solve_quadratic ---\n");
  {
    calc_complex_t r1;
    calc_complex_t r2;
    calc_status_t st;

    /* x^2-3x+2=0 -> 2, 1 */
    st = calculator_solve_quadratic(1.0f, -3.0f, 2.0f, &r1, &r2);
    printf("  x^2-3x+2=0   -> %.4f, %.4f %s\n", r1.real, r2.real,
           ((st == CALC_OK) && (fabsf(r1.real - 2.0f) < 1e-4f) &&
            (fabsf(r2.real - 1.0f) < 1e-4f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (fabsf(r1.real - 2.0f) < 1e-4f) &&
        (fabsf(r2.real - 1.0f) < 1e-4f)) { g_pass++; } else { g_fail++; }

    /* 2x^2-4x-6=0 -> 3, -1 */
    st = calculator_solve_quadratic(2.0f, -4.0f, -6.0f, &r1, &r2);
    printf("  2x^2-4x-6=0  -> %.4f, %.4f %s\n", r1.real, r2.real,
           ((st == CALC_OK) && (fabsf(r1.real - 3.0f) < 1e-4f) &&
            (fabsf(r2.real + 1.0f) < 1e-4f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (fabsf(r1.real - 3.0f) < 1e-4f) &&
        (fabsf(r2.real + 1.0f) < 1e-4f)) { g_pass++; } else { g_fail++; }

    /* x^2+2x+1=0 -> -1, -1 重根 */
    st = calculator_solve_quadratic(1.0f, 2.0f, 1.0f, &r1, &r2);
    printf("  x^2+2x+1=0   -> %.4f, %.4f %s\n", r1.real, r2.real,
           ((st == CALC_OK) && (fabsf(r1.real + 1.0f) < 1e-4f) &&
            (fabsf(r2.real + 1.0f) < 1e-4f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (fabsf(r1.real + 1.0f) < 1e-4f) &&
        (fabsf(r2.real + 1.0f) < 1e-4f)) { g_pass++; } else { g_fail++; }

    /* x^2+1=0 -> +i, -i */
    st = calculator_solve_quadratic(1.0f, 0.0f, 1.0f, &r1, &r2);
    printf("  x^2+1=0      -> %.4f%+.4fi, %.4f%+.4fi %s\n",
           r1.real, r1.imag, r2.real, r2.imag,
           ((st == CALC_OK) && (fabsf(r1.imag - 1.0f) < 1e-4f) &&
            (fabsf(r2.imag + 1.0f) < 1e-4f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (fabsf(r1.imag - 1.0f) < 1e-4f) &&
        (fabsf(r2.imag + 1.0f) < 1e-4f)) { g_pass++; } else { g_fail++; }

    /* x^2-2x+5=0 -> 1+2i, 1-2i */
    st = calculator_solve_quadratic(1.0f, -2.0f, 5.0f, &r1, &r2);
    printf("  x^2-2x+5=0   -> %.4f%+.4fi, %.4f%+.4fi %s\n",
           r1.real, r1.imag, r2.real, r2.imag,
           ((st == CALC_OK) && (fabsf(r1.real - 1.0f) < 1e-4f) &&
            (fabsf(r1.imag - 2.0f) < 1e-4f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (fabsf(r1.real - 1.0f) < 1e-4f) &&
        (fabsf(r1.imag - 2.0f) < 1e-4f)) { g_pass++; } else { g_fail++; }

    /* -x^2+1=0 -> ±1，x1 恒定是较大根 */
    st = calculator_solve_quadratic(-1.0f, 0.0f, 1.0f, &r1, &r2);
    printf("  -x^2+1=0     -> %.4f, %.4f %s\n", r1.real, r2.real,
           ((st == CALC_OK) && (fabsf(r1.real - 1.0f) < 1e-4f) &&
            (fabsf(r2.real + 1.0f) < 1e-4f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (fabsf(r1.real - 1.0f) < 1e-4f) &&
        (fabsf(r2.real + 1.0f) < 1e-4f)) { g_pass++; } else { g_fail++; }

    /* -x^2-1=0 -> a<0 且判别式<0，x1 应该带正虚部 */
    st = calculator_solve_quadratic(-1.0f, 0.0f, -1.0f, &r1, &r2);
    printf("  -x^2-1=0     -> %.4f%+.4fi, %.4f%+.4fi %s\n",
           r1.real, r1.imag, r2.real, r2.imag,
           ((st == CALC_OK) && (r1.imag > 0.0f) && (r2.imag < 0.0f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (r1.imag > 0.0f) && (r2.imag < 0.0f)) { g_pass++; } else { g_fail++; }

    /* 0x^2+2x-6=0 -> 退化成一次方程，两根都是 3 */
    st = calculator_solve_quadratic(0.0f, 2.0f, -6.0f, &r1, &r2);
    printf("  0x^2+2x-6=0  -> %.4f, %.4f %s\n", r1.real, r2.real,
           ((st == CALC_OK) && (fabsf(r1.real - 3.0f) < 1e-4f)) ? "ok" : "FAIL");
    if ((st == CALC_OK) && (fabsf(r1.real - 3.0f) < 1e-4f)) { g_pass++; } else { g_fail++; }

    /* 0x^2+0x+5=0 -> 无解 */
    st = calculator_solve_quadratic(0.0f, 0.0f, 5.0f, &r1, &r2);
    printf("  0x^2+0x+5=0  -> status=%d %s\n", (int)st,
           (st == CALC_DOMAIN) ? "ok" : "FAIL");
    if (st == CALC_DOMAIN) { g_pass++; } else { g_fail++; }
  }

  printf("\n================ %d passed, %d failed ================\n", g_pass, g_fail);
  return (g_fail == 0) ? 0 : 1;
}
