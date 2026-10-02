#ifndef CALCULATOR_ENGINE_H
#define CALCULATOR_ENGINE_H

#include <stdint.h>

typedef enum
{
  CALC_ANGLE_DEG = 0,
  CALC_ANGLE_RAD
} calc_angle_unit_t;

typedef struct
{
  float real;
  float imag;
} calc_complex_t;

typedef enum
{
  CALC_OK = 0,
  CALC_SYNTAX,
  CALC_DOMAIN,
  CALC_DIV_ZERO
} calc_status_t;

calc_status_t calculator_evaluate(const char *expression,
                                  calc_angle_unit_t angle_unit,
                                  uint8_t allow_complex,
                                  calc_complex_t answer,
                                  calc_complex_t *result);

calc_status_t calculator_solve_linear(float a, float b, float *x);

/* 解一元二次方程 a*x^2 + b*x + c = 0。
 *
 * x1 恒定是较大的实根（实根情形），x2 是另一个；判别式为负时 x1 带正虚部。
 * a == 0 时退化成一次方程，两个根相同。
 * 返回 CALC_OK；a == 0 且 b == 0 且 c != 0 时返回 CALC_DOMAIN（无解）。 */
calc_status_t calculator_solve_quadratic(float a, float b, float c,
                                         calc_complex_t *x1,
                                         calc_complex_t *x2);

#endif
