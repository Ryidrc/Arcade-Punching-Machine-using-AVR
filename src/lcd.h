#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* ----- Pin mapping SEMUA di PORTC ----- */

#define LCD_PORT      PORTC
#define LCD_DDR       DDRC

/* Control pins */
#define PIN_RS PC0
#define PIN_E  PC1
#define RS_MASK (1 << PIN_RS)
#define E_MASK  (1 << PIN_E)

/* Data pins D4..D7 */
#define D4 PC2
#define D5 PC3
#define D6 PC4
#define D7 PC5
#define DATA_MASK ((1<<D4)|(1<<D5)|(1<<D6)|(1<<D7))

/* ---------------- low-level ---------------- */

static void lcd_put_nibble(uint8_t nibble);

static void lcd_pulse_e(void);

static void lcd_send_byte(uint8_t byte);

static void lcd_cmd(uint8_t cmd) ;

static void lcd_data(uint8_t data);

/* ---------------- high-level ---------------- */

void lcd_clear(void);

void lcd_home(void);

void lcd_goto(uint8_t col, uint8_t row);

void lcd_puts(const char *s);

void lcd_putnum(int32_t num);

/* ---------------- initialization ---------------- */

void initlcd(void);