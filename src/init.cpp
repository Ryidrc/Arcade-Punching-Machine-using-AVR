#include <avr/io.h>     
#include <util/delay.h>   
#include <avr/interrupt.h> 
#include <stdint.h> 
#include <stdlib.h>
#include "DMD.h"
#include "SystemFont5x7.h"
#include "Arial_black_16.h"
#include "UART.h"
#include "init.h"

// Loadcell
#define HX_DOUT PD5   // Digital input
#define HX_SCK  PD4   // Digital output
#define TIMER1_COMPARE_VALUE 499 


// =======================================================
// =================== INITIALIZATION ====================
// =======================================================

void Hardware_Init(void) {
    // --- SENSOR (PD2 / INT0) ---
    DDRD &= ~(1 << DDD2);    // Input
    PORTD |= (1 << PD2);     // Pull-up
    
    // --- BUTTON (PD3 / INT1) ---
    DDRD &= ~(1 << DDD3);    // Input
    PORTD |= (1 << PD3);     // Pull-up

    // --- Interrupt Logic ---
    // ISC01 = 1, ISC00 = 0 -> INT0 Falling Edge (Sensor)
    // ISC11 = 1, ISC10 = 0 -> INT1 Falling Edge (Button)
    EICRA |= (1 << ISC01) | (1 << ISC11);
    
    // Enable both Interrupts
    EIMSK |= (1 << INT0) | (1 << INT1);
}


void hx711_init(void) {
    // DOUT as input
    DDRD &= ~(1 << HX_DOUT);

    // SCK as output
    DDRD |= (1 << HX_SCK);
    PORTD &= ~(1 << HX_SCK);   // Start low
}

long hx711_read(void) {
    long value = 0;
    while (PIND & (1 << HX_DOUT)); // Wait for ready

    uint8_t oldSREG = SREG;
    cli(); // Disable interrupts during the 24-bit read

    for (uint8_t i = 0; i < 24; i++) {
        PORTD |= (1 << HX_SCK);
        _delay_us(1);
        value = value << 1;
        PORTD &= ~(1 << HX_SCK);
        _delay_us(1);
        if (PIND & (1 << HX_DOUT)) {
            value++;
        }
    }

    // 3 pulses for Channel A (Gain 64)
    // Use 1 pulse for Channel A (Gain 128)
    // Use 2 pulses for Channel B (Gain 32)
    for (uint8_t i = 0; i < 3; i++) {
        PORTD |= (1 << HX_SCK);
        _delay_us(1);
        PORTD &= ~(1 << HX_SCK);
        _delay_us(1);
    }

    SREG = oldSREG; // Re-enable interrupts

    if (value & 0x800000) value |= 0xFF000000; // Sign extend
    return value;
}

void sys_init() {
    cli(); // Matikan interupsi global

    // A. Setup Timer 1 (CTC Mode) untuk P10 & System Tick
    OCR1A  = TIMER1_COMPARE_VALUE;
    TCCR1B |= (1 << WGM12); // CTC Mode
    TCCR1B |= (1 << CS11) | (1 << CS10); // Prescaler 64
    TIMSK1 |= (1 << OCIE1A); // Enable Interrupt

    sei(); // Nyalakan interupsi global
}