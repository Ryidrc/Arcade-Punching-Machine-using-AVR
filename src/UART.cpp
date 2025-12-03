#include <avr/io.h>     
#include <util/delay.h>   
#include <avr/interrupt.h> 
#include <stdint.h> 
#include <stdlib.h>

// --- Definitions ---
#define F_CPU 16000000UL
#define USART_BAUDRATE 9600
#define BAUD_PRESCALER (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

// =======================================================
// =================== UART FUNCTIONS  ===================
// =======================================================

void USART_Init() {
  UBRR0H = BAUD_PRESCALER >> 8;
  UBRR0L = BAUD_PRESCALER;
  UCSR0A = 0x00;
  UCSR0C = (1<<UCSZ01) | (1<<UCSZ00); 
  UCSR0B = (1<<RXEN0) | (1<<TXEN0);
}

void USART_TransmitPolling(uint8_t DataByte) {
  while (( UCSR0A & (1<<UDRE0)) == 0) {};
  UDR0 = DataByte;
}

void USART_PrintString(const char* str) {
    while (*str) {
        USART_TransmitPolling(*str);
        str++;
    }
}

void USART_PrintNumber(uint32_t num) {
    char buffer[11]; 
    ltoa(num, buffer, 10); 
    USART_PrintString(buffer);
}




