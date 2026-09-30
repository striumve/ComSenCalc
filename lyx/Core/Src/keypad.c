#include "keypad.h"
#include "main.h"

/* ---------------------------------------------------------------------------
 * TTP229-BSF x2, 2-wire serial interface.
 *
 * Protocol facts (from the TONTEK datasheet and the reference Arduino driver):
 *   - SCL is a clock *input* on the chip: the MCU drives it, and it idles HIGH.
 *   - SDO is the chip's data *output*, and it is ACTIVE LOW (low == touched).
 *   - A frame is 16 bits, LSB first: the first bit clocked out is channel TP0.
 *   - SDO also acts as a data-valid (DV) line: it rests HIGH and pulses LOW
 *     when a detection frame is ready.
 *   - Clock is a ~2 us low pulse followed by a ~2 us high pulse (~500 kHz).
 *   - After a frame the chip needs a short recovery time (Tout) before the
 *     next frame can be read; reading every 10 ms satisfies this.
 *
 * Both chips share GPIOB, so all four pins are configured here rather than
 * relying on the CubeMX-generated settings (which use LOW output speed and
 * would be lost on the next code generation anyway).
 * ------------------------------------------------------------------------- */

#define TTP_SCL1_PIN GPIO_PIN_6 /* PB6 */
#define TTP_SDO1_PIN GPIO_PIN_7 /* PB7 */
#define TTP_SCL2_PIN GPIO_PIN_8 /* PB8 */
#define TTP_SDO2_PIN GPIO_PIN_9 /* PB9 */

#define TTP_PORT GPIOB

#define TTP_FRAME_BITS    16U
#define TTP_KEYS_PER_CHIP 15U
#define TTP_CHIP_MASK     0x7FFFU /* bits 0..14 valid, bit 15 unused */

/* Set to 1 to gate each frame on the SDO data-valid pulse. Leave at 0 while
 * polling on a fixed schedule: the DV pulse repeats only at the chip's
 * sampling rate, so polling would miss it most of the time. */
#define TTP_USE_DV_GATE 0

#if TTP_USE_DV_GATE
#define TTP_DV_TIMEOUT_LOOPS 5000U
#endif

/* --- microsecond delay ---------------------------------------------------
 * Uses the Cortex-M3 DWT cycle counter. TIM3 is already reserved by screen.c
 * and is not reentrant, so it must not be shared between tasks. */
static void keypad_dwt_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void keypad_delay_us(uint32_t us)
{
  uint32_t cycles = us * (SystemCoreClock / 1000000U);
  uint32_t start = DWT->CYCCNT;

  while ((DWT->CYCCNT - start) < cycles)
  {
  }
}

static void keypad_gpio_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  /* SCL lines: push-pull outputs, idle high. Medium speed keeps the ~500 kHz
   * edges clean; CubeMX generates LOW speed, which is marginal here. */
  HAL_GPIO_WritePin(TTP_PORT, TTP_SCL1_PIN | TTP_SCL2_PIN, GPIO_PIN_SET);
  gpio.Pin = TTP_SCL1_PIN | TTP_SCL2_PIN;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(TTP_PORT, &gpio);

  /* SDO lines: driven by the TTP229 (push-pull), so no pull resistor. */
  gpio.Pin = TTP_SDO1_PIN | TTP_SDO2_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(TTP_PORT, &gpio);
}

static uint16_t ttp229_read_frame(uint16_t sclPin, uint16_t sdoPin)
{
  uint16_t frame = 0U;
  uint8_t i;

#if TTP_USE_DV_GATE
  uint32_t guard = TTP_DV_TIMEOUT_LOOPS;

  /* SDO rests high; the chip pulses it low when a frame is available. */
  while (HAL_GPIO_ReadPin(TTP_PORT, sdoPin) != GPIO_PIN_RESET)
  {
    if (--guard == 0U)
    {
      return 0U;
    }
  }

  guard = TTP_DV_TIMEOUT_LOOPS;
  while (HAL_GPIO_ReadPin(TTP_PORT, sdoPin) != GPIO_PIN_SET)
  {
    if (--guard == 0U)
    {
      return 0U;
    }
  }

  keypad_delay_us(10U); /* Tw */
#endif

  /* 16 bits, LSB first, sampled while SCL is low. SDO low means touched. */
  for (i = 0U; i < TTP_FRAME_BITS; i++)
  {
    HAL_GPIO_WritePin(TTP_PORT, sclPin, GPIO_PIN_RESET);
    keypad_delay_us(2U);

    if (HAL_GPIO_ReadPin(TTP_PORT, sdoPin) == GPIO_PIN_RESET)
    {
      frame |= (uint16_t)(1U << i);
    }

    HAL_GPIO_WritePin(TTP_PORT, sclPin, GPIO_PIN_SET);
    keypad_delay_us(2U);
  }

  return frame;
}

void keypad_init(void)
{
  keypad_dwt_init();
  keypad_gpio_init();
}

uint32_t touch_raw_read(void)
{
  uint32_t chip1 = (uint32_t)ttp229_read_frame(TTP_SCL1_PIN, TTP_SDO1_PIN);
  uint32_t chip2 = (uint32_t)ttp229_read_frame(TTP_SCL2_PIN, TTP_SDO2_PIN);

  /* chip #1 -> global keys 0..14, chip #2 -> global keys 15..29 */
  return (chip1 & TTP_CHIP_MASK) | ((chip2 & TTP_CHIP_MASK) << TTP_KEYS_PER_CHIP);
}

uint16_t keypad_debug_frame(uint8_t chip)
{
  if (chip == 1U)
  {
    return ttp229_read_frame(TTP_SCL1_PIN, TTP_SDO1_PIN);
  }
  if (chip == 2U)
  {
    return ttp229_read_frame(TTP_SCL2_PIN, TTP_SDO2_PIN);
  }
  return 0U;
}

uint8_t keypad_first_key(uint32_t bitmap)
{
  uint8_t i;

  for (i = 0U; i < KEYPAD_KEY_COUNT; i++)
  {
    if ((bitmap & (1UL << i)) != 0UL)
    {
      return i;
    }
  }
  return 0xFFU;
}
