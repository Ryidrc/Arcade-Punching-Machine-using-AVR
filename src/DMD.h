/*--------------------------------------------------------------------------------------
 DMD_AVR.h - Pure AVR Bare Metal Library for P10 LED Matrix
 Ported from Freetronics DMD Library
 Target: ATmega328P (Arduino Uno/Nano)
--------------------------------------------------------------------------------------*/
#ifndef DMD_AVR_H_
#define DMD_AVR_H_

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ======================================================================
// HARDWARE PIN DEFINITIONS (BARE METAL MAPPING)
// Sesuaikan pin ini dengan skematik Anda (Default: Arduino Uno Layout)
// ======================================================================

// --- PORT B DEFINITIONS (PB0 - PB5) ---
#define DMD_PORT_B_REG  PORTB
#define DMD_DDR_B_REG   DDRB
#define PIN_DMD_SCLK    PB0  // Arduino D8 (Latch)
#define PIN_DMD_nOE     PB1  // Arduino D9 (Output Enable)
#define PIN_SPI_SS      PB2  // Arduino D10 (Chip Select lain)
#define PIN_SPI_MOSI    PB3  // Arduino D11 (Data)
#define PIN_SPI_SCK     PB5  // Arduino D13 (Clock)

// --- PORT D DEFINITIONS (PD6 - PD7) ---
#define DMD_PORT_D_REG  PORTD
#define DMD_DDR_D_REG   DDRD
#define PIN_DMD_A       PD6  // Arduino D6
#define PIN_DMD_B       PD7  // Arduino D7

// ======================================================================
// MACROS: FAST PORT MANIPULATION
// ======================================================================

// Output Enable Control
#define OE_DMD_ROWS_OFF()   { DMD_PORT_B_REG &= ~(1 << PIN_DMD_nOE); }
#define OE_DMD_ROWS_ON()    { DMD_PORT_B_REG |=  (1 << PIN_DMD_nOE); }

// Latch Control (SCLK) - Pulse High then Low
#define LATCH_DMD_SHIFT_REG_TO_OUTPUT() { \
    DMD_PORT_B_REG |=  (1 << PIN_DMD_SCLK); \
    DMD_PORT_B_REG &= ~(1 << PIN_DMD_SCLK); \
}

// Row Selection Macros (A & B pins)
// Row 1,5,9,13: A=0, B=0
#define LIGHT_DMD_ROW_01_05_09_13() { \
    DMD_PORT_D_REG &= ~((1 << PIN_DMD_A) | (1 << PIN_DMD_B)); \
}
// Row 2,6,10,14: A=1, B=0
#define LIGHT_DMD_ROW_02_06_10_14() { \
    DMD_PORT_D_REG = (DMD_PORT_D_REG & ~(1 << PIN_DMD_B)) | (1 << PIN_DMD_A); \
}
// Row 3,7,11,15: A=0, B=1
#define LIGHT_DMD_ROW_03_07_11_15() { \
    DMD_PORT_D_REG = (DMD_PORT_D_REG & ~(1 << PIN_DMD_A)) | (1 << PIN_DMD_B); \
}
// Row 4,8,12,16: A=1, B=1
#define LIGHT_DMD_ROW_04_08_12_16() { \
    DMD_PORT_D_REG |= (1 << PIN_DMD_A) | (1 << PIN_DMD_B); \
}

// Graphics Modes
#define GRAPHICS_NORMAL    0
#define GRAPHICS_INVERSE   1
#define GRAPHICS_TOGGLE    2
#define GRAPHICS_OR        3
#define GRAPHICS_NOR       4

// Test Patterns
#define PATTERN_ALT_0     0
#define PATTERN_ALT_1     1
#define PATTERN_STRIPE_0  2
#define PATTERN_STRIPE_1  3

// Display Specs
#define DMD_PIXELS_ACROSS         32      
#define DMD_PIXELS_DOWN           16      
#define DMD_BITSPERPIXEL           1      
#define DMD_RAM_SIZE_BYTES        ((DMD_PIXELS_ACROSS*DMD_BITSPERPIXEL/8)*DMD_PIXELS_DOWN)

// Lookup table for pixel bitmask (Stored in header for static access or move to cpp)
static const uint8_t bPixelLookupTable[8] = {
   0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

// Font Indices
#define FONT_LENGTH             0
#define FONT_FIXED_WIDTH        2
#define FONT_HEIGHT             3
#define FONT_FIRST_CHAR         4
#define FONT_CHAR_COUNT         5
#define FONT_WIDTH_TABLE        6

// ======================================================================
// CLASS DEFINITION
// ======================================================================
class DMD {
  public:
    DMD(uint8_t panelsWide, uint8_t panelsHigh);
    
    // Core Graphics
    void writePixel(unsigned int bX, unsigned int bY, uint8_t bGraphicsMode, uint8_t bPixel);
    void drawString(int bX, int bY, const char* bChars, uint8_t length, uint8_t bGraphicsMode);
    void selectFont(const uint8_t* font);
    int  drawChar(const int bX, const int bY, const unsigned char letter, uint8_t bGraphicsMode);
    int  charWidth(const unsigned char letter);
    
    // Marquee / Scrolling
    void drawMarquee(const char* bChars, uint8_t length, int left, int top);
    bool stepMarquee(int amountX, int amountY); // bool is standard in C++ (or include stdbool.h for C)

    // Shapes & Helpers
    void clearScreen(uint8_t bNormal);
    void drawLine(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode);
    void drawCircle(int xCenter, int yCenter, int radius, uint8_t bGraphicsMode);
    void drawBox(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode);
    void drawFilledBox(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode);
    void drawTestPattern(uint8_t bPattern);

    // Hardware Driver
    void scanDisplayBySPI();

  private:
    void drawCircleSub(int cx, int cy, int x, int y, uint8_t bGraphicsMode);
    void spi_init_bare();
    inline void spi_transfer_bare(uint8_t data);

    uint8_t *bDMDScreenRAM;

    char marqueeText[256];
    uint8_t marqueeLength;
    int marqueeWidth;
    int marqueeHeight;
    int marqueeOffsetX;
    int marqueeOffsetY;

    const uint8_t* Font;

    uint8_t DisplaysWide;
    uint8_t DisplaysHigh;
    uint8_t DisplaysTotal;
    int row1, row2, row3;

    volatile uint8_t bDMDByte;
};

#endif /* DMD_AVR_H_ */