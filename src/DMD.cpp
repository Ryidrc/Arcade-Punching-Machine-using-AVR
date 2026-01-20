#include "DMD.h"

DMD::DMD(uint8_t panelsWide, uint8_t panelsHigh)
{
    DisplaysWide = panelsWide;
    DisplaysHigh = panelsHigh;
    DisplaysTotal = DisplaysWide * DisplaysHigh;
    row1 = DisplaysTotal << 4;
    row2 = DisplaysTotal << 5;
    row3 = ((DisplaysTotal << 2) * 3) << 2;
    
    // Allocate RAM using malloc (standard C)
    bDMDScreenRAM = (uint8_t *) malloc(DisplaysTotal * DMD_RAM_SIZE_BYTES);

    // 1. SETUP GPIO (BARE METAL)
    // Set Direction Registers to OUTPUT (1)
    DMD_DDR_D_REG |= (1 << PIN_DMD_A) | (1 << PIN_DMD_B);
    DMD_DDR_B_REG |= (1 << PIN_DMD_nOE) | (1 << PIN_DMD_SCLK) | (1 << PIN_SPI_MOSI) | (1 << PIN_SPI_SCK) | (1 << PIN_SPI_SS);

    // Initialize Pin States (LOW default, OE Off/Low initially)
    DMD_PORT_D_REG &= ~((1 << PIN_DMD_A) | (1 << PIN_DMD_B));
    DMD_PORT_B_REG &= ~((1 << PIN_DMD_SCLK) | (1 << PIN_SPI_SCK) | (1 << PIN_DMD_nOE)); 
    DMD_PORT_B_REG |= (1 << PIN_SPI_SS); // SS High (Inactive)

    // 2. SETUP SPI (BARE METAL)
    spi_init_bare();

    clearScreen(true);
    bDMDByte = 0;
}

void DMD::spi_init_bare() {
    // Enable SPI, Master, set clock rate fck/4
    // SPE: SPI Enable
    // MSTR: Master Select
    // SPR0: Clock Divide (Optional, default div 4)
    SPCR = (1 << SPE) | (1 << MSTR); 
    SPSR = (1 << SPI2X); // Double speed (fck/2) -> Max speed for P10
}

inline void DMD::spi_transfer_bare(uint8_t data) {
    SPDR = data; // Start transmission
    while (!(SPSR & (1 << SPIF))); // Wait for transmission complete
}

void DMD::writePixel(unsigned int bX, unsigned int bY, uint8_t bGraphicsMode, uint8_t bPixel)
{
    unsigned int uiDMDRAMPointer;

    if (bX >= (DMD_PIXELS_ACROSS * DisplaysWide) || bY >= (DMD_PIXELS_DOWN * DisplaysHigh)) {
        return;
    }
    uint8_t panel = (bX / DMD_PIXELS_ACROSS) + (DisplaysWide * (bY / DMD_PIXELS_DOWN));
    bX = (bX % DMD_PIXELS_ACROSS) + (panel << 5);
    bY = bY % DMD_PIXELS_DOWN;
    
    uiDMDRAMPointer = bX / 8 + bY * (DisplaysTotal << 2);
    uint8_t lookup = bPixelLookupTable[bX & 0x07];

    switch (bGraphicsMode) {
    case GRAPHICS_NORMAL:
        if (bPixel == 1) // true
            bDMDScreenRAM[uiDMDRAMPointer] &= ~lookup; 
        else
            bDMDScreenRAM[uiDMDRAMPointer] |= lookup; 
        break;
    case GRAPHICS_INVERSE:
        if (bPixel == 0) // false
            bDMDScreenRAM[uiDMDRAMPointer] &= ~lookup; 
        else
            bDMDScreenRAM[uiDMDRAMPointer] |= lookup; 
        break;
    case GRAPHICS_TOGGLE:
        if (bPixel == 1) {
            if ((bDMDScreenRAM[uiDMDRAMPointer] & lookup) == 0)
                bDMDScreenRAM[uiDMDRAMPointer] |= lookup;
            else
                bDMDScreenRAM[uiDMDRAMPointer] &= ~lookup;
        }
        break;
    case GRAPHICS_OR:
        if (bPixel == 1) bDMDScreenRAM[uiDMDRAMPointer] &= ~lookup;
        break;
    case GRAPHICS_NOR:
        if ((bPixel == 1) && ((bDMDScreenRAM[uiDMDRAMPointer] & lookup) == 0))
            bDMDScreenRAM[uiDMDRAMPointer] |= lookup;
        break;
    }
}

void DMD::scanDisplayBySPI()
{
    // Cek Pin CS lain (Bare Metal check PINB Register)
    // Jika PIN_SPI_SS LOW, berarti device lain sedang pakai SPI.
    if (! (PINB & (1 << PIN_SPI_SS)) ) {
        // SS Low -> Device lain aktif, kita minggir dulu.
        // (Asumsi SS sebagai Input indikator, jika Anda pakai sebagai Output CS device lain, logic ini perlu dibalik)
        return; 
    }

    int rowsize = DisplaysTotal << 2;
    int offset = rowsize * bDMDByte;

    // SPI Transfer Loop
    for (int i = 0; i < rowsize; i++) {
        spi_transfer_bare(bDMDScreenRAM[offset + i + row3]);
        spi_transfer_bare(bDMDScreenRAM[offset + i + row2]);
        spi_transfer_bare(bDMDScreenRAM[offset + i + row1]);
        spi_transfer_bare(bDMDScreenRAM[offset + i]);
    }

    OE_DMD_ROWS_OFF();
    LATCH_DMD_SHIFT_REG_TO_OUTPUT();

    switch (bDMDByte) {
        case 0: LIGHT_DMD_ROW_01_05_09_13(); bDMDByte = 1; break;
        case 1: LIGHT_DMD_ROW_02_06_10_14(); bDMDByte = 2; break;
        case 2: LIGHT_DMD_ROW_03_07_11_15(); bDMDByte = 3; break;
        case 3: LIGHT_DMD_ROW_04_08_12_16(); bDMDByte = 0; break;
    }
    OE_DMD_ROWS_ON();
}

void DMD::clearScreen(uint8_t bNormal)
{
    if (bNormal)
        memset(bDMDScreenRAM, 0xFF, DMD_RAM_SIZE_BYTES * DisplaysTotal);
    else
        memset(bDMDScreenRAM, 0x00, DMD_RAM_SIZE_BYTES * DisplaysTotal);
}

void DMD::selectFont(const uint8_t * font) {
    this->Font = font;
}

void DMD::drawString(int bX, int bY, const char *bChars, uint8_t length, uint8_t bGraphicsMode)
{
    if (bX >= (DMD_PIXELS_ACROSS * DisplaysWide) || bY >= DMD_PIXELS_DOWN * DisplaysHigh) return;
    uint8_t height = pgm_read_byte(this->Font + FONT_HEIGHT);
    if (bY + height < 0) return;

    int strWidth = 0;
    this->drawLine(bX - 1, bY, bX - 1, bY + height, GRAPHICS_INVERSE);

    for (int i = 0; i < length; i++) {
        int charWide = this->drawChar(bX + strWidth, bY, bChars[i], bGraphicsMode);
        if (charWide > 0) {
            strWidth += charWide;
            this->drawLine(bX + strWidth, bY, bX + strWidth, bY + height, GRAPHICS_INVERSE);
            strWidth++;
        } else if (charWide < 0) {
            return;
        }
        if ((bX + strWidth) >= DMD_PIXELS_ACROSS * DisplaysWide || bY >= DMD_PIXELS_DOWN * DisplaysHigh) return;
    }
}

int DMD::drawChar(const int bX, const int bY, const unsigned char letter, uint8_t bGraphicsMode)
{
    if (bX > (DMD_PIXELS_ACROSS * DisplaysWide) || bY > (DMD_PIXELS_DOWN * DisplaysHigh)) return -1;
    unsigned char c = letter;
    uint8_t height = pgm_read_byte(this->Font + FONT_HEIGHT);
    if (c == ' ') {
        int charWide = charWidth(' ');
        this->drawFilledBox(bX, bY, bX + charWide, bY + height, GRAPHICS_INVERSE);
        return charWide;
    }
    uint8_t width = 0;
    uint8_t bytes = (height + 7) / 8;
    uint8_t firstChar = pgm_read_byte(this->Font + FONT_FIRST_CHAR);
    uint8_t charCount = pgm_read_byte(this->Font + FONT_CHAR_COUNT);
    uint16_t index = 0;

    if (c < firstChar || c >= (firstChar + charCount)) return 0;
    c -= firstChar;

    if (pgm_read_byte(this->Font + FONT_LENGTH) == 0 && pgm_read_byte(this->Font + FONT_LENGTH + 1) == 0) {
        width = pgm_read_byte(this->Font + FONT_FIXED_WIDTH);
        index = c * bytes * width + FONT_WIDTH_TABLE;
    } else {
        for (uint8_t i = 0; i < c; i++) {
            index += pgm_read_byte(this->Font + FONT_WIDTH_TABLE + i);
        }
        index = index * bytes + charCount + FONT_WIDTH_TABLE;
        width = pgm_read_byte(this->Font + FONT_WIDTH_TABLE + c);
    }
    if (bX < -width || bY < -height) return width;

    for (uint8_t j = 0; j < width; j++) { 
        for (uint8_t i = bytes - 1; i < 254; i--) { 
            uint8_t data = pgm_read_byte(this->Font + index + j + (i * width));
            int offset = (i * 8);
            if ((i == bytes - 1) && bytes > 1) { offset = height - 8; }
            for (uint8_t k = 0; k < 8; k++) { 
                if ((offset + k >= i * 8) && (offset + k <= height)) {
                    if (data & (1 << k)) {
                        writePixel(bX + j, bY + offset + k, bGraphicsMode, true);
                    } else {
                        writePixel(bX + j, bY + offset + k, bGraphicsMode, false);
                    }
                }
            }
        }
    }
    return width;
}

int DMD::charWidth(const unsigned char letter) {
    unsigned char c = letter;
    if (c == ' ') c = 'n';
    uint8_t width = 0;
    uint8_t firstChar = pgm_read_byte(this->Font + FONT_FIRST_CHAR);
    uint8_t charCount = pgm_read_byte(this->Font + FONT_CHAR_COUNT);
    
    if (c < firstChar || c >= (firstChar + charCount)) return 0;
    c -= firstChar;

    if (pgm_read_byte(this->Font + FONT_LENGTH) == 0 && pgm_read_byte(this->Font + FONT_LENGTH + 1) == 0) {
        width = pgm_read_byte(this->Font + FONT_FIXED_WIDTH);
    } else {
        width = pgm_read_byte(this->Font + FONT_WIDTH_TABLE + c);
    }
    return width;
}

void DMD::drawMarquee(const char *bChars, uint8_t length, int left, int top) {
    marqueeWidth = 0;
    for (int i = 0; i < length; i++) {
        marqueeText[i] = bChars[i];
        marqueeWidth += charWidth(bChars[i]) + 1;
    }
    marqueeHeight = pgm_read_byte(this->Font + FONT_HEIGHT);
    marqueeText[length] = '\0';
    marqueeOffsetY = top;
    marqueeOffsetX = left;
    marqueeLength = length;
    drawString(marqueeOffsetX, marqueeOffsetY, marqueeText, marqueeLength, GRAPHICS_NORMAL);
}

bool DMD::stepMarquee(int amountX, int amountY) {
    bool ret = false;
    marqueeOffsetX += amountX;
    marqueeOffsetY += amountY;
    if (marqueeOffsetX < -marqueeWidth) {
        marqueeOffsetX = DMD_PIXELS_ACROSS * DisplaysWide;
        clearScreen(true);
        ret = true;
    } else if (marqueeOffsetX > DMD_PIXELS_ACROSS * DisplaysWide) {
        marqueeOffsetX = -marqueeWidth;
        clearScreen(true);
        ret = true;
    }
    if (marqueeOffsetY < -marqueeHeight) {
        marqueeOffsetY = DMD_PIXELS_DOWN * DisplaysHigh;
        clearScreen(true);
        ret = true;
    } else if (marqueeOffsetY > DMD_PIXELS_DOWN * DisplaysHigh) {
        marqueeOffsetY = -marqueeHeight;
        clearScreen(true);
        ret = true;
    }
    drawString(marqueeOffsetX, marqueeOffsetY, marqueeText, marqueeLength, GRAPHICS_NORMAL);
    return ret;
}

void DMD::drawLine(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode) {
    int dy = y2 - y1;
    int dx = x2 - x1;
    int stepx, stepy;
    if (dy < 0) { dy = -dy; stepy = -1; } else { stepy = 1; }
    if (dx < 0) { dx = -dx; stepx = -1; } else { stepx = 1; }
    dy <<= 1; dx <<= 1;
    writePixel(x1, y1, bGraphicsMode, true);
    if (dx > dy) {
        int fraction = dy - (dx >> 1);
        while (x1 != x2) {
            if (fraction >= 0) { y1 += stepy; fraction -= dx; }
            x1 += stepx; fraction += dy;
            writePixel(x1, y1, bGraphicsMode, true);
        }
    } else {
        int fraction = dx - (dy >> 1);
        while (y1 != y2) {
            if (fraction >= 0) { x1 += stepx; fraction -= dy; }
            y1 += stepy; fraction += dx;
            writePixel(x1, y1, bGraphicsMode, true);
        }
    }
}

void DMD::drawBox(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode) {
    drawLine(x1, y1, x2, y1, bGraphicsMode);
    drawLine(x2, y1, x2, y2, bGraphicsMode);
    drawLine(x2, y2, x1, y2, bGraphicsMode);
    drawLine(x1, y2, x1, y1, bGraphicsMode);
}

void DMD::drawFilledBox(int x1, int y1, int x2, int y2, uint8_t bGraphicsMode) {
    for (int b = x1; b <= x2; b++) {
        drawLine(b, y1, b, y2, bGraphicsMode);
    }
}

void DMD::drawCircle(int xCenter, int yCenter, int radius, uint8_t bGraphicsMode) {
    int x = 0; int y = radius; int p = (5 - radius * 4) / 4;
    drawCircleSub(xCenter, yCenter, x, y, bGraphicsMode);
    while (x < y) {
        x++;
        if (p < 0) { p += 2 * x + 1; } else { y--; p += 2 * (x - y) + 1; }
        drawCircleSub(xCenter, yCenter, x, y, bGraphicsMode);
    }
}

void DMD::drawCircleSub(int cx, int cy, int x, int y, uint8_t bGraphicsMode) {
    if (x == 0) {
        writePixel(cx, cy + y, bGraphicsMode, true); writePixel(cx, cy - y, bGraphicsMode, true);
        writePixel(cx + y, cy, bGraphicsMode, true); writePixel(cx - y, cy, bGraphicsMode, true);
    } else if (x == y) {
        writePixel(cx + x, cy + y, bGraphicsMode, true); writePixel(cx - x, cy + y, bGraphicsMode, true);
        writePixel(cx + x, cy - y, bGraphicsMode, true); writePixel(cx - x, cy - y, bGraphicsMode, true);
    } else if (x < y) {
        writePixel(cx + x, cy + y, bGraphicsMode, true); writePixel(cx - x, cy + y, bGraphicsMode, true);
        writePixel(cx + x, cy - y, bGraphicsMode, true); writePixel(cx - x, cy - y, bGraphicsMode, true);
        writePixel(cx + y, cy + x, bGraphicsMode, true); writePixel(cx - y, cy + x, bGraphicsMode, true);
        writePixel(cx + y, cy - x, bGraphicsMode, true); writePixel(cx - y, cy - x, bGraphicsMode, true);
    }
}

void DMD::drawTestPattern(uint8_t bPattern) {
    unsigned int ui;
    int numPixels = DisplaysTotal * DMD_PIXELS_ACROSS * DMD_PIXELS_DOWN;
    int pixelsWide = DMD_PIXELS_ACROSS * DisplaysWide;
    for (ui = 0; ui < numPixels; ui++) {
        switch (bPattern) {
        case PATTERN_ALT_0:
            if ((ui & pixelsWide) == 0) writePixel((ui & (pixelsWide - 1)), ((ui & ~(pixelsWide - 1)) / pixelsWide), GRAPHICS_NORMAL, ui & 1);
            else writePixel((ui & (pixelsWide - 1)), ((ui & ~(pixelsWide - 1)) / pixelsWide), GRAPHICS_NORMAL, !(ui & 1));
            break;
        case PATTERN_ALT_1:
            if ((ui & pixelsWide) == 0) writePixel((ui & (pixelsWide - 1)), ((ui & ~(pixelsWide - 1)) / pixelsWide), GRAPHICS_NORMAL, !(ui & 1));
            else writePixel((ui & (pixelsWide - 1)), ((ui & ~(pixelsWide - 1)) / pixelsWide), GRAPHICS_NORMAL, ui & 1);
            break;
        case PATTERN_STRIPE_0:
            writePixel((ui & (pixelsWide - 1)), ((ui & ~(pixelsWide - 1)) / pixelsWide), GRAPHICS_NORMAL, ui & 1);
            break;
        case PATTERN_STRIPE_1:
            writePixel((ui & (pixelsWide - 1)), ((ui & ~(pixelsWide - 1)) / pixelsWide), GRAPHICS_NORMAL, !(ui & 1));
            break;
        }
    }
}
