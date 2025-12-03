#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include "lcd.h"



static void lcd_put_nibble(uint8_t nibble) {
    LCD_PORT &= ~DATA_MASK;

    if (nibble & 0x01) LCD_PORT |= (1<<D4);
    if (nibble & 0x02) LCD_PORT |= (1<<D5);
    if (nibble & 0x04) LCD_PORT |= (1<<D6);
    if (nibble & 0x08) LCD_PORT |= (1<<D7);
}

static void lcd_pulse_e(void) {
    LCD_PORT |= E_MASK;
    _delay_us(1);
    LCD_PORT &= ~E_MASK;
    _delay_us(50);
}

static void lcd_send_byte(uint8_t byte) {
    lcd_put_nibble((byte >> 4) & 0x0F);
    lcd_pulse_e();

    lcd_put_nibble(byte & 0x0F);
    lcd_pulse_e();
}

static void lcd_cmd(uint8_t cmd) {
    LCD_PORT &= ~RS_MASK;
    lcd_send_byte(cmd);

    if (cmd == 0x01 || cmd == 0x02)
        _delay_ms(2);
    else
        _delay_us(50);
}

static void lcd_data(uint8_t data) {
    LCD_PORT |= RS_MASK;
    lcd_send_byte(data);
    _delay_us(50);
}

/* ---------------- high-level ---------------- */

void lcd_clear(void) {
    lcd_cmd(0x01);
}

void lcd_home(void) {
    lcd_cmd(0x02);
}

void lcd_goto(uint8_t col, uint8_t row) {
    const uint8_t offsets[] = {0x00, 0x40, 0x14, 0x54};
    lcd_cmd(0x80 | (col + offsets[row]));
}

void lcd_puts(const char *s) {
    while (*s) lcd_data(*s++);
}

void lcd_putnum(int32_t num) {
    char buf[12];  // cukup untuk -2147483648
    int i = 0;

    // Jika nol langsung cetak "0"
    if (num == 0) {
        lcd_data('0');
        return;
    }

    // Handle negatif
    if (num < 0) {
        lcd_data('-');
        num = -num;
    }

    // Konversi manual (reverse)
    while (num > 0 && i < sizeof(buf)-1) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    // Print ulang secara terbalik
    while (i > 0) {
        lcd_data(buf[--i]);
    }
}

/* ---------------- initialization ---------------- */

void initlcd(void) {
    LCD_DDR |= RS_MASK | E_MASK | DATA_MASK;

    LCD_PORT &= ~(RS_MASK | E_MASK | DATA_MASK);

    _delay_ms(40);

    lcd_put_nibble(0x03);
    lcd_pulse_e();
    _delay_ms(5);

    lcd_put_nibble(0x03);
    lcd_pulse_e();
    _delay_us(150);

    lcd_put_nibble(0x03);
    lcd_pulse_e();
    _delay_us(150);

    lcd_put_nibble(0x02);
    lcd_pulse_e();
    _delay_us(150);

    lcd_cmd(0x28);  // 4-bit, 2-line
    lcd_cmd(0x0C);  // Display ON, cursor OFF
    lcd_cmd(0x06);  // Entry mode
    lcd_clear();
}
