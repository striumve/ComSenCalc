#include "keypad.h"
#include "main.h"

/* ---------------------------------------------------------------------------
 * 两片TTP229-BSF
 * ------------------------------------------------------------------------- */

#define TTP_SCL_A_MASK GPIO_PIN_6 /* PB6 */
#define TTP_SDO_A_MASK GPIO_PIN_7 /* PB7 */
#define TTP_SCL_B_MASK GPIO_PIN_8 /* PB8 */
#define TTP_SDO_B_MASK GPIO_PIN_9 /* PB9 */

#define TTP_PORT GPIOB

#define TTP_FRAME_BITS 16U
#define TTP_HALF_PERIOD_US 4U

static const uint8_t a_channel_to_key[TTP_FRAME_BITS] = {
    7U, 11U, 6U, 1U, 0U, 5U, 10U, 2U, 0xFFU, 14U, 9U, 4U, 3U, 8U, 13U, 12U
};

static const uint8_t b_channel_to_key[TTP_FRAME_BITS] = {
    27U, 15U, 20U, 25U, 16U, 21U, 26U, 17U, 22U, 28U, 23U, 18U, 29U, 24U, 19U, 0xFFU
};

/* --- 微秒级延时 ----------------------------------------------------------- */
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

  /* SCL线：推挽输出，空闲为高 */
  HAL_GPIO_WritePin(TTP_PORT, TTP_SCL_A_MASK | TTP_SCL_B_MASK, GPIO_PIN_SET);
  gpio.Pin = TTP_SCL_A_MASK | TTP_SCL_B_MASK;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(TTP_PORT, &gpio);

  /* SDO线 */
  gpio.Pin = TTP_SDO_A_MASK | TTP_SDO_B_MASK;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(TTP_PORT, &gpio);
}

static uint16_t ttp229_read_chip_speed(uint16_t scl_mask, uint16_t sdo_mask,
                                       uint16_t half_us)
{
  uint16_t value = 0U;
  uint8_t i;

  for (i = 0U; i < TTP_FRAME_BITS; i++)
  {
    TTP_PORT->BSRR = (uint32_t)scl_mask << 16; /* SCL 拉低 */
    keypad_delay_us(half_us);

    if ((TTP_PORT->IDR & sdo_mask) == 0U) /* 低电平有效 */
    {
      value |= (uint16_t)(1U << i);
    }

    TTP_PORT->BSRR = scl_mask; /* SCL 拉高 */
    keypad_delay_us(half_us);
  }

  return value;
}

static uint16_t ttp229_read_chip(uint16_t scl_mask, uint16_t sdo_mask)
{
  return ttp229_read_chip_speed(scl_mask, sdo_mask, TTP_HALF_PERIOD_US);
}

void keypad_init(void)
{
  keypad_dwt_init();
  keypad_gpio_init();
}

uint32_t touch_raw_read(void)
{
  uint16_t chip_a = ttp229_read_chip(TTP_SCL_A_MASK, TTP_SDO_A_MASK);
  uint16_t chip_b = ttp229_read_chip(TTP_SCL_B_MASK, TTP_SDO_B_MASK);
  uint32_t keys = 0U;
  uint8_t channel;

  for (channel = 0U; channel < TTP_FRAME_BITS; channel++)
  {
    if (((chip_a >> channel) & 1U) != 0U)
    {
      uint8_t key = a_channel_to_key[channel];
      if (key < KEYPAD_KEY_COUNT)
      {
        keys |= (1UL << key);
      }
    }
    if (((chip_b >> channel) & 1U) != 0U)
    {
      uint8_t key = b_channel_to_key[channel];
      if (key < KEYPAD_KEY_COUNT)
      {
        keys |= (1UL << key);
      }
    }
  }

  return keys;
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
