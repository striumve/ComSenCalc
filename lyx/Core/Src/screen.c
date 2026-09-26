#include "screen.h"

/* TIM3 is configured as a 1 MHz up-counter (72 MHz / (71 + 1)). */
extern TIM_HandleTypeDef htim3;

#define LCD_DATA_PINS (LCD_D4_PIN | LCD_D5_PIN | LCD_D6_PIN | LCD_D7_PIN)
#define LCD_DATA_SHIFT 12U

#define LCD_CMD_CLEAR 0x01U
#define LCD_CMD_HOME 0x02U
#define LCD_CMD_ENTRY 0x06U
#define LCD_CMD_DISPLAY 0x0CU
#define LCD_CMD_FUNC 0x28U
#define LCD_CMD_OFF 0x08U
#define LCD_CMD_LINE1 0x80U
#define LCD_CMD_LINE2 0xC0U

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

  /* Wake-up sequence: force 8-bit mode three times, then switch to 4-bit. */
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
