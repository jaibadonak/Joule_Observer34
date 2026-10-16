/*
 * uart.c
 *
 * Created: 14/10/2026 5:08:13 pm
 *  Author: Admin
 */ 

 
#include "uart.h"    // Include our UART function declarations
#include <avr/io.h>  // Gives us access to ATmega328PB hardware registers

// Define the CPU clock speed if it has not already been defined elsewhere.
// This must match the actual clock frequency used by the microcontroller.
#ifndef F_CPU
#define F_CPU 2000000UL
#endif

// Required UART communication speed: 9600 baud
#define UART_BAUD 9600UL

// Calculate the UART baud-rate register value.
// The ATmega UART uses a clock divider when operating in normal mode.
#define UBRR_VALUE ((F_CPU / (16UL * UART_BAUD)) - 1UL)



void uart_init(void)
{
    // Set the upper 8 bits of the baud-rate register.
    // The >> 8 operation shifts the value right by 8 bits.
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);

    // Set the lower 8 bits of the baud-rate register.
    // This determines the actual communication speed.
    UBRR0L = (uint8_t)UBRR_VALUE;

    // Enable the UART transmitter.
    // TXEN0 is the bit that allows the UART to send data.
    UCSR0B = (1 << TXEN0);

    // Configure the UART for 8 data bits, no parity and 1 stop bit (8N1).
    // UCSZ01 and UCSZ00 together select 8-bit data.
    // Parity and the second stop bit remain disabled.
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}



void uart_transmit(char data)
{
    
    while (!(UCSR0A & (1 << UDRE0)))
    {
        // Do nothing while waiting for the transmitter.
    }

    // Write the character to the UART data register.
    // The hardware then sends the byte serially through the TX pin.
    UDR0 = data;
}


void uart_string(const char *str)
{
    // C strings end with a null character: '\0'.
    // Continue transmitting until the end of the string is reached.
    while (*str != '\0')
    {
        // Send the character currently pointed to by str.
        uart_transmit(*str);

        // Advance the pointer to the next character in the string.
        str++;
    }
}



void uart_int32(int32_t value)
{
    // Temporary storage for the decimal digits.
    // Digits are initially stored in reverse order.
    char digits[10];

    // Tracks how many digits have been stored.
    uint8_t count = 0;

    // Stores the absolute magnitude of the number as an unsigned value.
    uint32_t magnitude;

    // Check whether the number is negative.
    if (value < 0)
    {
        // Transmit the minus sign first.
        uart_transmit('-');

        // Calculate the magnitude safely, including INT32_MIN.
        // Negating INT32_MIN directly would overflow a signed int32_t.
        magnitude = (uint32_t)(-(value + 1)) + 1U;
    }
    else
    {
        // Non-negative values can be converted directly.
        magnitude = (uint32_t)value;
    }

    // Extract digits from right to left using division and remainder.
    // The do-while ensures that zero is transmitted as "0", not nothing.
    do
    {
        // % 10 gives the final decimal digit.
        // Adding '0' converts the digit 0-9 into its text character.
        digits[count++] = '0' + (magnitude % 10U);

        // Remove the final digit by dividing by 10.
        magnitude /= 10U;
    }
    while (magnitude > 0U);

    // The digits were stored backwards, so transmit them in reverse order.
    while (count > 0U)
    {
        // Decrement count, then transmit the next digit.
        uart_transmit(digits[--count]);
    }
}


void uart_send_measurements(int32_t vrms_mV,
                            int32_t irms_mA,
                            int32_t power_mW)
{
    // Start the message with "$M," to identify it as measurement data.
    uart_string("$M,");

    // Send voltage as an integer in millivolts.
    uart_int32(vrms_mV);

    // Comma separates the voltage and current fields.
    uart_transmit(',');

    // Send current as an integer in milliamps.
    uart_int32(irms_mA);

    // Comma separates the current and power fields.
    uart_transmit(',');

    // Send power as an integer in milliwatts.
    uart_int32(power_mW);

    // End the message with carriage return and newline.
    // This lets the receiver recognise the end of the message.
    uart_string("\r\n");
}
