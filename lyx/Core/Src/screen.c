#include "screen.h"

/* TIM3 被配置成 1MHz 递增计数器（72MHz / (71 + 1)）。 */
extern TIM_HandleTypeDef htim3;

#define LCD_DATA_PINS (LCD_D4_PIN | LCD_D5_PIN | LCD_D6_PIN | LCD_D7_PIN)
#define LCD_DATA_SHIFT 12U

#define LCD_CMD_CLEAR 0x01U
#define LCD_CMD_HOME 0x02U
#define LCD_CMD_ENTRY 0x06U
#define LCD_CMD_DISPLAY 0x0CU         /* 开显示，关光标           */
#define LCD_CMD_DISPLAY_CURSOR 0x0EU  /* 开显示，开光标，不闪烁   */
#define LCD_CMD_FUNC 0x28U
#define LCD_CMD_OFF 0x08U
#define LCD_CMD_LINE1 0x80U
#define LCD_CMD_LINE2 0xC0U
#define LCD_CMD_CGRAM 0x40U           /* 设置 CGRAM 地址          */

/* 自定义字形 */
#define LCD_CUSTOM_COUNT 4U

static const uint8_t lcd_custom_chars[LCD_CUSTOM_COUNT][8] = {
    /* 0x01: π */
    {0x00U, 0x00U, 0x1FU, 0x0AU, 0x0AU, 0x0AU, 0x0AU, 0x00U},
    /* 0x02: × */
    {0x00U, 0x00U, 0x11U, 0x0AU, 0x04U, 0x0AU, 0x11U, 0x00U},
    /* 0x03: ÷ */
    {0x00U, 0x04U, 0x00U, 0x1FU, 0x00U, 0x04U, 0x00U, 0x00U},
    /* 0x04: √ */
    {0x00U, 0x07U, 0x04U, 0x04U, 0x04U, 0x14U, 0x18U, 0x08U},
};

static void lcd_delay_us(uint16_t us)
{
  __HAL_TIM_SET_COUNTER(&htim3, 0U);
  while (__HAL_TIM_GET_COUNTER(&htim3) < us)
  {
  }
}

static void lcd_write_data_pins(uint8_t nibble)
{
  uint32_t set = (uint32_t)(nibble & 0x0FU) << LCD_DATA_SHIFT;
  LCD_PORT->BSRR = ((uint32_t)LCD_DATA_PINS << 16) | set;
}

static void lcd_set_rs(uint8_t selected)
{
  LCD_PORT->BSRR = selected ? LCD_RS_PIN : ((uint32_t)LCD_RS_PIN << 16);
}

static void lcd_pulse_enable(void)
{
  LCD_PORT->BSRR = LCD_E_PIN;
  lcd_delay_us(2U);
  LCD_PORT->BSRR = (uint32_t)LCD_E_PIN << 16;
  lcd_delay_us(2U);
}

static void lcd_write_nibble(uint8_t nibble)
{
  lcd_write_data_pins(nibble);
  lcd_pulse_enable();
}

static void lcd_write_byte(uint8_t value, uint8_t rs)
{
  lcd_set_rs(rs);
  lcd_write_nibble((uint8_t)(value >> 4));
  lcd_write_nibble((uint8_t)(value & 0x0FU));
  lcd_delay_us(50U);
}

static void lcd_write_command(uint8_t command)
{
  lcd_write_byte(command, 0U);
  if ((command == LCD_CMD_CLEAR) || (command == LCD_CMD_HOME))
  {
    HAL_Delay(2U);
  }
}

static void lcd_write_char(char c)
{
  lcd_write_byte((uint8_t)c, 1U);
}

static void lcd_write_line(const char *text)
{
  for (uint8_t i = 0U; i < LCD_COLUMNS; i++)
  {
    lcd_write_char((text[i] != '\0') ? text[i] : ' ');
  }
}

void screen_init(void)
{
  LCD_PORT->BSRR = ((uint32_t)(LCD_RS_PIN | LCD_E_PIN | LCD_DATA_PINS) << 16);

  HAL_Delay(50U);

  /* 唤醒序列：先强行三次 8 位模式，再切到 4 位模式。 */
  lcd_write_nibble(0x03U);
  HAL_Delay(5U);
  lcd_write_nibble(0x03U);
  lcd_delay_us(150U);
  lcd_write_nibble(0x03U);
  lcd_delay_us(150U);
  lcd_write_nibble(0x02U);

  lcd_write_command(LCD_CMD_FUNC);
  lcd_write_command(LCD_CMD_OFF);
  lcd_write_command(LCD_CMD_CLEAR);
  lcd_write_command(LCD_CMD_ENTRY);
  lcd_write_command(LCD_CMD_DISPLAY);

  screen_load_custom_chars();
}

void screen_load_custom_chars(void)
{
  uint8_t index;
  uint8_t row;

  for (index = 0U; index < LCD_CUSTOM_COUNT; index++)
  {
    /* 字符码 = index + 1（跳过 0x00），CGRAM 地址 = 码 << 3 */
    uint8_t code = (uint8_t)(index + 1U);

    lcd_write_command((uint8_t)(LCD_CMD_CGRAM | ((uint8_t)(code << 3))));
    for (row = 0U; row < 8U; row++)
    {
      lcd_write_char((char)(lcd_custom_chars[index][row] & 0x1FU));
    }
  }

  /* 指针必须回到 DDRAM，否则后面的显示内容会被写进 CGRAM。 */
  lcd_write_command(LCD_CMD_LINE1);
}

void screen_clear(void)
{
  lcd_write_command(LCD_CMD_CLEAR);
}

void screen_write_lines(const char *line1, const char *line2)
{
  lcd_write_command(LCD_CMD_LINE1);
  lcd_write_line(line1);
  lcd_write_command(LCD_CMD_LINE2);
  lcd_write_line(line2);
}

void screen_write_frame(const char *line1, const char *line2,
                        uint8_t cursor_enabled, uint8_t cursor_row,
                        uint8_t cursor_column)
{
  uint8_t address = (cursor_row != 0U) ? LCD_CMD_LINE2 : LCD_CMD_LINE1;

  screen_write_lines(line1, line2);

  if (cursor_enabled != 0U)
  {
    lcd_write_command((uint8_t)(address + (cursor_column & 0x0FU)));
    lcd_write_command(LCD_CMD_DISPLAY_CURSOR);
  }
  else
  {
    lcd_write_command(LCD_CMD_DISPLAY);
  }
}
