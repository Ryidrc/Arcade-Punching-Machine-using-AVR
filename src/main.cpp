#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

// Project headers (assumed present)
#include "DMD.h"
#include "SystemFont5x7.h"
#include "Arial_black_16.h"
#include "UART.h"
#include "lcd.h"
#include "init.h"

// -------------------------------------------------------------------------
// CONFIG / GLOBALS
// -------------------------------------------------------------------------
DMD led_module(1, 1); // 1x1 panel

// Game state
volatile bool gameActive = false;
int highScore = 0;

// ISR synchronization
volatile uint16_t msCounter = 0;          // ms accumulator
volatile int8_t  gameTimer = 0;           // countdown in seconds
volatile bool    updateScreen = false;    // request screen redraw
volatile unsigned long system_ticks = 0;  // simple millis replacement

// Idle animation
int xPunch = 0, dirPunch = 1;
int xGame = 10, dirGame = -1;
unsigned long lastAnimTime = 0;

// HX711 / coin / scoring variables
int TimerForGame = 20; // default 20s
volatile uint32_t sensor_counter = 0;

long tare_value[160];
long average_tare = 0;
long hit_value = 0;
long score = 0;           // raw peak value
int display_score = 0;    // 0..100 percent shown on display
bool waiting_for_release = false;

long stored_score = 0;
bool has_score = false;

bool counting = false;

volatile uint8_t g_sensor_event = 0;
volatile uint8_t g_button_event = 0;

// Function prototypes (implementation provided in other files)
void sys_init(void);
void delay_soft_ms(unsigned long ms);
void tampilkanIdleBergerak(void);
void handleGameLogic(void);
void tampilkanHighScore(void);

void USART_Init(void);
void Hardware_Init(void);
void hx711_init(void);
long hx711_read(void);
void USART_PrintString(const char* str);
void USART_PrintNumber(uint32_t num);


void initlcd(void);
void lcd_clear(void);
void lcd_home(void);
void lcd_putc(char c);
void lcd_puts(const char *s);
void lcd_goto(uint8_t x, uint8_t y);
void lcd_putnum(int32_t num);


// -------------------------------------------------------------------------
// INTERRUPTS
// -------------------------------------------------------------------------
ISR(INT0_vect) {
    EIMSK &= ~(1 << INT0);  // Disable Sensor Interrupt while handling
    g_sensor_event = 1;
    sensor_counter++;
}

ISR(INT1_vect) {
    EIMSK &= ~(1 << INT1);
    g_button_event = 1;
}

// Timer1 Compare A ISR: drives display refresh and timekeeping
ISR(TIMER1_COMPA_vect) {
    // Refresh display (non-blocking)
    led_module.scanDisplayBySPI();

    // system tick increments every 2 ms in your original design
    system_ticks += 2;

    if (gameActive) {
        msCounter += 2;
        if (msCounter >= 1000) {
            msCounter = 0;
            if (gameTimer >= 0) {
                gameTimer--;
                updateScreen = true;
            }
        }
    }
}

// -------------------------------------------------------------------------
// MAIN
// -------------------------------------------------------------------------
int main(void) {
    uint32_t last_printed_count = 0;
    uint32_t current_count_copy = 0;

    // Init subsystems (assumed provided)
    sys_init();
    led_module.clearScreen(true);
    USART_Init();
    Hardware_Init();
    hx711_init();
    initlcd();

    lcd_puts("Insert Coin!");

    // Compute average tare
    average_tare = 0;
    for (int i = 0; i < 160; i++) {
        tare_value[i] = hx711_read();
        average_tare += tare_value[i];
        _delay_ms(1);
    }
    average_tare = average_tare / 160;
    _delay_ms(200);
    hit_value = average_tare - 400000;
    hit_value = labs(hit_value);

    USART_PrintString("--- System Ready: Sensor(PD2) & Button(PD3) ---\r\n");

    USART_PrintNumber(average_tare);
    USART_PrintString("\r\n");
    USART_PrintNumber(hit_value);
    USART_PrintString("\r\n");

    sei(); // enable global interrupts

    // Main  
    while (1) {
        // If no game active, show idle animation
        if (!gameActive) {
            tampilkanIdleBergerak();
        }

        // If a game is active, run game loop:
        if (gameActive) {
            // 1) Check for screen update request (driven by timer ISR)
            if (updateScreen) {
                updateScreen = false;

                // If time expired, show game over and return to idle
                if (gameTimer < 0) {
                    USART_PrintString("Waktu Habis.\n");
                    led_module.clearScreen(true);
                    led_module.selectFont(SystemFont5x7);
                    led_module.drawString(6, 4, "TIME", 4, GRAPHICS_NORMAL);
                    delay_soft_ms(1000);
                    led_module.clearScreen(true);
                    led_module.drawString(6, 4, "OVER", 4, GRAPHICS_NORMAL);
                    delay_soft_ms(2000);

                    // reset state and go to IDLE
                    gameActive = false;
                    counting = false;
                    score = 0;
                    display_score = 0;
                    has_score = false;
                    continue; // go to idle
                }

                // Draw countdown number
                char buf[8];
                itoa(gameTimer, buf, 10);
                led_module.clearScreen(true);
                int xPos = (gameTimer >= 10) ? 5 : 11;
                led_module.drawString(xPos, 0, buf, strlen(buf), GRAPHICS_NORMAL);

                USART_PrintString("Waktu: ");
                USART_PrintNumber(gameTimer);
                USART_PrintString("\n");
            }

            // 2) Read sensor (polling)
            long raw = hx711_read();
            raw = labs(raw);
            USART_PrintNumber(raw);
            USART_PrintString("\n");
            _delay_ms(1);

            // detect start of hit
            if (raw > hit_value && counting == false) {
                counting = true;
                score = 0;
            }

            // during hit: track max
            if (counting) {
                if (raw > score) score = raw;

                // clamp max to 8,000,000 (as in original)
                if (score >= 8000000) score = 8000000;

                // compute 0..100 percentage properly using integer math:
                display_score = (int)(((score * 100L) / (800000 - 1))); // 0..999

            }

            // end of hit: raw dropped below threshold -> finalize
            if (raw <= hit_value && counting == true) {
                counting = false;

                stored_score = score;
                has_score = true;

                // show score in UART
                USART_PrintString("Score: ");
                USART_PrintNumber(display_score);
                USART_PrintString("\n");
                USART_PrintString("\nRaw Score: ");
                USART_PrintNumber(score);
                USART_PrintString("\n");
                USART_PrintString("\nHit Value: ");
                USART_PrintNumber(hit_value);
                USART_PrintString("\n");

                // Visual score-up animation on DMD
                char buf[8];
                int displayScore = 0;
                int step = display_score / 40;
                if (step < 1) step = 1;

                while (displayScore < display_score) {
                    displayScore += step;
                    if (displayScore > display_score) displayScore = display_score;

                    led_module.clearScreen(true);
                    int xPos = (displayScore < 10) ? 11 : (displayScore < 100) ? 5 : 1;
                    itoa(displayScore, buf, 10);
                    led_module.drawString(xPos, 0, buf, strlen(buf), GRAPHICS_NORMAL);

                    delay_soft_ms(30);
                }
                delay_soft_ms(2000);

                // check high score and show appropriate screen
                if (display_score > highScore) {
                    highScore = display_score;
                    USART_PrintString(">> NEW HIGH SCORE! <<\n");
                    tampilkanHighScore();
                } else {
                    led_module.clearScreen(true);
                    led_module.selectFont(SystemFont5x7);
                    led_module.drawString(6, 0, "HIGH", 4, GRAPHICS_NORMAL);
                    led_module.drawString(2, 8, "SCORE", 5, GRAPHICS_NORMAL);
                    delay_soft_ms(2000);

                    led_module.clearScreen(true);
                    led_module.selectFont(Arial_Black_16);
                    int hxPos = (highScore < 10) ? 11 : (highScore < 100) ? 5 : 1;
                    itoa(highScore, buf, 10);
                    led_module.drawString(hxPos, 0, buf, strlen(buf), GRAPHICS_NORMAL);
                    delay_soft_ms(3000);
                }

                // After hit processed, stop the game (go back to idle)
                gameActive = false;
                score = 0;
                display_score = 0;
                has_score = false;
            }
        } // end if gameActive

        // HANDLE SENSOR EVENT (INT0) POST-PROCESS
        static unsigned long sensorBlindTimer = 0;

        if (g_sensor_event == 1) {
            // Start the timer if it hasn't started
            if (sensorBlindTimer == 0) {
                sensorBlindTimer = system_ticks;
            }

            // Check if 1000ms has passed
            if (system_ticks - sensorBlindTimer > 1000) {
                EIFR |= (1 << INTF0); // Clear interrupt flag
                EIMSK |= (1 << INT0); // Re-enable Interrupt
                g_sensor_event = 0;   // Reset event flag
                sensorBlindTimer = 0; // Reset timer
            }
        }

        // PART 1: Handle the Press (Triggered by Interrupt)
        if (g_button_event == 1) {
            g_button_event = 0; // Reset flag immediately
            
            // Check if button is truly pressed (LOW) to filter noise
            if ( (PIND & (1 << PD3)) == 0 ) {
                
                // --- YOUR GAME START LOGIC HERE ---
                cli();
                if (sensor_counter == 0) {
                    USART_PrintString("Insert Coin!\r\n");
                } else {
                    sensor_counter--;
                    USART_PrintString("Game Started!\r\n");
                    
                    gameActive = true;
                    gameTimer = TimerForGame;
                    msCounter = 0;
                    updateScreen = true;
                    
                    led_module.clearScreen(true);
                    led_module.selectFont(Arial_Black_16);
                }
                sei();
                // ----------------------------------

                // CRITICAL CHANGE: Do NOT re-enable interrupt yet!
                // Instead, tell the system to wait for the button to be released.
                waiting_for_release = true;
            } else {
                // If it was just noise (pin is HIGH), re-enable immediately
                EIFR |= (1 << INTF1);
                EIMSK |= (1 << INT1);
            }
        }

        // PART 2: Handle the Release (Polling)
        if (waiting_for_release) {
            static unsigned long releaseTimer = 0;
            
            // Check if the button is HIGH (Released)
            if (PIND & (1 << PD3)) { 
                // Use a timer to ensure it is STABLE high (debounce the release)
                if (releaseTimer == 0) {
                    releaseTimer = system_ticks; // Start timer
                }
                
                // If it stays HIGH for 50ms, we confirm it's released
                if (system_ticks - releaseTimer > 50) {
                    waiting_for_release = false; // Stop waiting
                    releaseTimer = 0;            // Reset timer
                    
                    // NOW it is safe to re-enable the interrupt for the next press
                    EIFR |= (1 << INTF1); // Clear any flags generated while holding
                    EIMSK |= (1 << INT1); // Re-enable INT1
                }
            } else {
                // If button goes LOW again (bouncing or still held), reset timer
                releaseTimer = 0;
            }
        }   

        // TASK 3: Print coin counter changes
        cli();
        current_count_copy = sensor_counter;
        sei();

        if (current_count_copy != last_printed_count) {
            USART_PrintString("Counter: ");
            lcd_goto(0,1);
            lcd_puts("Credit: ");
            
            USART_PrintNumber(current_count_copy);
            lcd_putnum(current_count_copy);

            USART_PrintString("\r\n");
            last_printed_count = current_count_copy;
        }
    } // end main loop

    return 0;
}

void tampilkanIdleBergerak() {
    // Non-blocking delay menggunakan system_ticks
    if (system_ticks - lastAnimTime > 80) { 
        lastAnimTime = system_ticks;
        
        led_module.clearScreen(true); 
        led_module.selectFont(SystemFont5x7);
        
        led_module.drawString(xPunch, 0, "PUNCH", 5, GRAPHICS_NORMAL);
        xPunch += dirPunch;
        if (xPunch >= 3) dirPunch = -1; 
        if (xPunch <= 0) dirPunch = 1;  
        
        led_module.drawString(xGame, 8, "GAME", 4, GRAPHICS_NORMAL);
        xGame += dirGame;
        if (xGame >= 9) dirGame = -1; 
        if (xGame <= 0) dirGame = 1;  
    }
}

void tampilkanHighScore() {
    led_module.clearScreen(true);
    led_module.selectFont(SystemFont5x7);
    const char *text = "HIGHEST SCORE!   ";
    led_module.drawMarquee(text, strlen(text), 32, 4);
    
    unsigned long start = system_ticks;
    while (system_ticks - start < 6000) { // Putar selama 6 detik
        unsigned long timer = system_ticks;
        bool ret = false;
        while (!ret) {
            if ((timer + 40) < system_ticks) { 
                ret = led_module.stepMarquee(-1, 0);
                timer = system_ticks;
            }
        }
    }
}

void delay_soft_ms(unsigned long ms) {
    unsigned long start = system_ticks;
    while (system_ticks - start < ms); // Busy wait
}
