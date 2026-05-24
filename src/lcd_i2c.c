#include "lcd_i2c.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <string.h>

#define LCD_I2C i2c0
#define LCD_SDA_PIN 0
#define LCD_SCL_PIN 1
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE 0x04
#define LCD_RS 0x01

static uint8_t lcd_addr = LCD_ADDR_PRIMARY;
static bool lcd_present = false;

static bool i2c_write_byte(uint8_t val) {
    return i2c_write_blocking(LCD_I2C, lcd_addr, &val, 1, false) == 1;
}

static void lcd_pulse(uint8_t data) {
    i2c_write_byte(data | LCD_ENABLE | LCD_BACKLIGHT);
    sleep_us(1);
    i2c_write_byte((data & ~LCD_ENABLE) | LCD_BACKLIGHT);
    sleep_us(50);
}

static void lcd_write4(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT | mode;
    i2c_write_byte(data);
    lcd_pulse(data);
}

static void lcd_send(uint8_t value, uint8_t mode) {
    lcd_write4(value & 0xF0, mode);
    lcd_write4((value << 4) & 0xF0, mode);
}

static void lcd_cmd(uint8_t cmd) {
    lcd_send(cmd, 0);
    if (cmd == 0x01 || cmd == 0x02) sleep_ms(2);
}

static void lcd_data(uint8_t data) {
    lcd_send(data, LCD_RS);
}

bool lcd_is_present(void) {
    return lcd_present;
}

void lcd_init_auto(void) {
    i2c_init(LCD_I2C, 100 * 1000);
    gpio_set_function(LCD_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(LCD_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(LCD_SDA_PIN);
    gpio_pull_up(LCD_SCL_PIN);
    sleep_ms(60);

    uint8_t test = 0;
    lcd_addr = LCD_ADDR_PRIMARY;
    if (i2c_write_blocking(LCD_I2C, lcd_addr, &test, 1, false) != 1) {
        lcd_addr = LCD_ADDR_SECONDARY;
        if (i2c_write_blocking(LCD_I2C, lcd_addr, &test, 1, false) != 1) {
            lcd_present = false;
            return;
        }
    }

    lcd_present = true;

    sleep_ms(50);
    lcd_write4(0x30, 0);
    sleep_ms(5);
    lcd_write4(0x30, 0);
    sleep_us(150);
    lcd_write4(0x30, 0);
    lcd_write4(0x20, 0); // 4-bit

    lcd_cmd(0x28); // 2 lines, 5x8
    lcd_cmd(0x0C); // display on, cursor off
    lcd_cmd(0x06); // entry mode
    lcd_cmd(0x01); // clear
}

void lcd_clear(void) {
    if (!lcd_present) return;
    lcd_cmd(0x01);
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    if (!lcd_present) return;
    static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    lcd_cmd(0x80 | (col + row_offsets[row & 0x03]));
}

static void lcd_print_padded(const char *s) {
    char buf[17];
    size_t len = strlen(s);
    if (len > 16) len = 16;
    memset(buf, ' ', 16);
    memcpy(buf, s, len);
    buf[16] = 0;
    for (int i = 0; i < 16; i++) lcd_data((uint8_t)buf[i]);
}

void lcd_print(const char *s) {
    if (!lcd_present) return;
    while (*s) lcd_data((uint8_t)*s++);
}

void lcd_print2(const char *line1, const char *line2) {
    if (!lcd_present) return;
    lcd_set_cursor(0, 0);
    lcd_print_padded(line1);
    lcd_set_cursor(0, 1);
    lcd_print_padded(line2);
}
