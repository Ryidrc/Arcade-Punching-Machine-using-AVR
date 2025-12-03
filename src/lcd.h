#ifndef LCD_H_
#define LCD_H_

#include <avr/io.h>
#include <stdbool.h>

// --- All Control and Data Pins are on PORTC ---
#define LCD_PORT PORTC
#define LCD_DDR DDRC

// Control Pin Definitions (Connected to the high nibble of PORTC)
#define RS PC4 // Register Select
#define EN PC5 // Enable

// Data Pin Shift (D4-D7 connected to PC0-PC3)
// Since the data is connected to PC0-PC3, the shift is 0.
#define LCD_DATA_SHIFT 0 

// Command Cycle Definitions
#define LCD_1CYCLE 0
#define LCD_2CYCLE 1

// Function Prototypes
void LCD_putch(unsigned char data);
void LCD_putcmd(unsigned char data, unsigned char cmdtype);
void initlcd();
void lcd_puts(const char *s); // Corrected signature for string literals
void lcd_gotoxy(unsigned char x, unsigned char y);
void lcd_clear();
void lcd_putnum(int number);

#endif /* LCD_H_ */