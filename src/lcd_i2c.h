#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>
#include <stdbool.h>

#define LCD_ADDR_PRIMARY 0x27
#define LCD_ADDR_SECONDARY 0x3F

void lcd_init_auto(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_print(const char *s);
void lcd_print2(const char *line1, const char *line2);
bool lcd_is_present(void);

#endif
