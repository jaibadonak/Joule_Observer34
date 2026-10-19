/*
 * uart.c
 *
 * Created: 14/10/2026 5:08:13 pm
 *  Author: Admin
 */ 

 
#include "uart.h"   

#include <avr/io.h>
#include "config.h"
#include "adc.h"
#include "computation.h"

#define UART_BAUD 9600UL

#define UBRR_VALUE ((F_CPU / (16UL * UART_BAUD)) - 1UL)



void uart_init(void)
{
    UCSR0A = 0; // normal-speed asynchronous USART; do not inherit U2X from a bootloader
    // Set the upper 8 bits of the baud-rate register.
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);

    // Set the lower 8 bits of the baud-rate register.
    UBRR0L = (uint8_t)UBRR_VALUE;

    // Enable the UART transmitter.
    UCSR0B = (1 << TXEN0);

    // Configure the UART(8N1).
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}



void uart_transmit(char data)
{
    //wait till empty
    while (!(UCSR0A & (1 << UDRE0)))
    {
        // Do nothing while waiting for the transmitter
    }

    // Write the character to the UART data register.
    UDR0 = data;
}


void uart_string(const char *str)
{
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

        // Calculate the magnitude
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
        digits[count++] = '0' + (magnitude % 10U);

        // Remove the final digit by dividing by 10.
        magnitude /= 10U;
    }
    while (magnitude > 0U);

    // The digits were stored backwards so transmit in reverse
    while (count > 0U)
    {
        // Decrement count, then transmit the next digit.
        uart_transmit(digits[--count]);
    }
}


void uart_send_measurements(int32_t vrms_mV,
                            int32_t irms_mA,
                            int32_t power_mW,
                            uint32_t total_mWh,
                            uint32_t peak_mA)
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
    uart_transmit(',');
    uart_int32((int32_t)total_mWh);
    uart_transmit(',');
    uart_int32((int32_t)peak_mA);

    // End the message with carriage return and newline.
    // This lets the receiver recognise the end of the message.
    uart_string("\r\n");
}

void uart_send_waveform(void) {
    // Same cycle-based DC correction as scalar power. Raw ADC samples stay untouched.
    uart_string("$Y,104,"); uart_int32(adc_cycle_offset_x100(v_samples));
    uart_transmit(','); uart_int32(adc_cycle_offset_x100(i_samples));
    uart_transmit(','); uart_int32(V_MV_PER_COUNT_X100);
    uart_transmit(','); uart_int32(I_MA_PER_COUNT_X1000);
    for (uint16_t k = 0; k < samples; k++) {
        uart_transmit(','); uart_int32(v_samples[k]);
        uart_transmit(','); uart_int32(i_samples[k]);
    }
    uart_string("\r\n");
}

static void uart_calibration_channel(volatile uint16_t *block) {
    uint16_t lo = 1023, hi = 0;
    uint32_t total = 0;
    for (uint16_t k = 0; k < samples; k++) {
        uint16_t x = block[k];
        if (x < lo) lo = x;
        if (x > hi) hi = x;
        total += x;
    }
    uart_transmit(','); uart_int32(lo);
    uart_transmit(','); uart_int32(hi);
    uart_transmit(','); uart_int32(total * 100UL / samples);
    uart_transmit(','); uart_int32(adc_rms_counts_x10(block));
}

void uart_send_calibration(void) {
    uart_string("$C,"); uart_int32(ADC_REFERENCE_MV);
    uart_transmit(','); uart_int32(V_MV_PER_COUNT_X100);
    uart_transmit(','); uart_int32(I_MA_PER_COUNT_X1000);
    uart_calibration_channel(v_samples);
    uart_calibration_channel(i_samples);
    uart_transmit(','); uart_int32(adc_rms_counts_x100(v_samples));
    uart_transmit(','); uart_int32(adc_rms_counts_x100(i_samples));
    uart_string("\r\n");
}

void uart_send_mode(uint8_t mode) {
    if (mode >= 4) return;
    uart_string("$D,");
    uart_transmit('0' + mode);
    uart_string("\r\n");
}
