#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include "lcd.h"

// Macro to write a 4-bit nibble to the data lines (PC0-PC3)
// This simplifies LCD_putch and LCD_putcmd by handling the clear/write/pulse logic.
#define WRITE_NIBBLE(DataNibble) \
    /* Clear old data on PC0-PC3 */ \
    LCD_PORT &= ~0x0F; \
    /* Write new nibble to PC0-PC3. Nibble must be shifted right by 4 before macro. */ \
    LCD_PORT |= (DataNibble >> 4); \
    /* Pulse EN (PC5) high and low to execute */ \
    LCD_PORT |= (1<<EN); \
    _delay_us(1); \
    LCD_PORT &= ~(1<<EN); 

// --- Function Definitions ---

void LCD_putch(unsigned char data)
{
    // High Nibble (RS=1 for data)
    LCD_PORT |= (1<<RS);
    WRITE_NIBBLE(data & 0xF0);
    _delay_us(1);

    // Low Nibble (RS=1 for data)
    LCD_PORT |= (1<<RS);
    // Low nibble must be shifted left by 4 before the macro, 
    // where it will be shifted right by 4 back to PC0-PC3.
    WRITE_NIBBLE((data & 0x0F) << 4); 
    _delay_ms(5);
}

void LCD_putcmd(unsigned char data, unsigned char cmdtype)
{
    // High Nibble (RS=0 for command)
    LCD_PORT &= ~(1<<RS);
    WRITE_NIBBLE(data & 0xF0);
    _delay_us(1);

    if(cmdtype) // If it's a 2-cycle command, write the low nibble
    {
        // Low Nibble (RS=0 for command)
        LCD_PORT &= ~(1<<RS);
        // Low nibble must be shifted left by 4 before the macro.
        WRITE_NIBBLE((data & 0x0F) << 4); 
    }
    _delay_ms(5);
}

void initlcd()
{
    // Set PC0-PC5 as outputs (0x3F is 0b00111111).
    LCD_DDR |= (1<<RS) | (1<<EN) | 0x0F; 

    // Initialization Sequence (Timing is critical for 4-bit mode)
    _delay_ms(30);
    LCD_putcmd(0x30,LCD_1CYCLE);
    _delay_ms(8);
    LCD_putcmd(0x30,LCD_1CYCLE);
    _delay_us(200);
    LCD_putcmd(0x30,LCD_1CYCLE);
    LCD_putcmd(0x20,LCD_1CYCLE); // Switch to 4-bit mode
    
    // Final Configuration (2-cycle commands)
    LCD_putcmd(0x28,LCD_2CYCLE); // 4-bit, 2 lines, 5x8 dots
    LCD_putcmd(0x0C,LCD_2CYCLE); // Display ON, Cursor OFF, Blink OFF
    LCD_putcmd(0x01,LCD_2CYCLE); // Clear Display
    LCD_putcmd(0x06,LCD_2CYCLE); // Entry Mode Set (Increment, No Shift)
}

void lcd_puts(const char *s)
{
    while(*s != 0)
    {
        if(*s == '\n') 
            LCD_putcmd(0xC0,LCD_2CYCLE); // Go to Line 2 (DDRAM address 0x40)
        else 
            LCD_putch(*s);
        s++;
    }
}

void lcd_gotoxy (unsigned char x, unsigned char y)
{
    unsigned char Position = (1 << 7); // Start with DDRAM address command (0x80)

    // Note: The addresses below assume a standard 4-line display layout for y=2 and y=3.
    if (y==1) {
        Position |= 0x40;
    } else if (y==2) {
        Position |= 0x14;
    } else if (y==3) {
        Position |= 0x54;
    }
    
    // This line runs always to set the column offset (x)
    Position += x; 
    
    LCD_putcmd(Position, LCD_2CYCLE);
}

void lcd_clear()
{
    LCD_putcmd(0x01, LCD_2CYCLE);
} 

void lcd_putnum(int number)
{
    // Basic integer to string conversion (handles 0-999)
    unsigned char digit;
    
    // Hundreds
    digit = '0';
    while(number >= 100){
        digit++;
        number -= 100;
    }
    LCD_putch(digit);
    
    // Tens
    digit = '0';
    while(number >= 10)
    {
        digit++;
        number -= 10;
    }
    LCD_putch(digit);
    
    // Ones
    LCD_putch('0' + number);
}