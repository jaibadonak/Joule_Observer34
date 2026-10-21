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
#include "display.h"

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


//it turns a number into text and sends it over UART. 
//The UART can only send characters, so 467 has to go out as '4' '6' '7'.
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
                            uint32_t peak_mA,
                            uint32_t apparent_mVA,
                            int16_t power_factor_x1000,
                            uint32_t frequency_mHz)
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
    uart_transmit(','); uart_int32(apparent_mVA);
    uart_transmit(','); uart_int32(power_factor_x1000);
    uart_transmit(','); uart_int32(frequency_mHz);

    // End the message with carriage return and newline.
    // This lets the receiver recognise the end of the message.
    uart_string("\r\n");
}

// A begin/sample/stats/end stream keeps each line short and readable.
// No second waveform buffer is needed on the 2 KB AVR.
void uart_send_waveform(void) {
    // Work out each channel's DC level once, then use it for all samples in this capture.
    uint32_t voltage_dc = adc_cycle_offset_x100(v_samples);
    uint32_t current_dc = adc_cycle_offset_x100(i_samples);
    capture_stats_t stats[5] = {{0}}; // V, I, P, raw V, raw I
    uart_string("$B,"); uart_int32(ADC_SAMPLE_US);
    uart_transmit(','); uart_int32(samples); uart_string("\r\n");
    uint8_t last_mode = 255; // outside the valid modes, so the first mode is always sent
    // Send one row per sample position, using the same index for voltage and current.
    for (uint16_t k = 0; k < samples; k++) {
        waveform_sample_t value; // groups the calculated voltage, current and power together
        // &value passes its address so the function can fill in these three fields.
        calc_waveform_sample(k, voltage_dc, current_dc, &value);
        int32_t values[5] = {value.voltage_mV, value.current_mA, value.power_mW,
                            v_samples[k], i_samples[k]};
        uart_string("$S,"); uart_int32(k);
        // Send all five values for this position and update their statistics.
        for (uint8_t channel = 0; channel < 5; channel++) {
            capture_stats_add(&stats[channel], values[channel]);
            uart_transmit(','); uart_int32(values[channel]);
        }
        uart_string("\r\n");
        // The display scrolls while UART blocks; keep its mode report current.
        uint8_t mode = display_current_quantity();
        if (mode != last_mode) { uart_send_mode(mode); last_mode = mode; }
    }
    // After all samples, send min, max, mean and RMS for each of the five channels.
    for (uint8_t channel = 0; channel < 5; channel++) {
        uart_string("$G,"); uart_int32(channel);
        uart_transmit(','); uart_int32(stats[channel].minimum);
        uart_transmit(','); uart_int32(stats[channel].maximum);
        uart_transmit(','); uart_int32(capture_stats_mean(&stats[channel]));
        uart_transmit(','); uart_int32(capture_stats_rms(&stats[channel]));
        uart_string("\r\n");
    }
    uart_string("$E\r\n"); // end marker: ESP32 can now publish the complete capture
}

// Find raw ADC statistics for one channel and append them to the calibration packet.
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

// Send gain settings and raw statistics used by the website's calibration panel.
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

// Send the local display's selected measurement as a digit, 0 to 3.
void uart_send_mode(uint8_t mode) {
    if (mode >= 4) return;
    uart_string("$D,");
    uart_transmit('0' + mode); // turn a number such as 2 into its printable character '2'
    uart_string("\r\n");
}

// USB serial diagnostics are sent through the ESP32.
// block points to either v_samples or i_samples; name labels the printed line.
static void send_raw_stats(const char *name, volatile uint16_t *block) {
    // Start at opposite ends of the 10-bit range so the first sample replaces both limits.
    uint16_t lo = 1023, hi = 0;
    uint32_t total = 0;
    for (uint16_t k = 0; k < samples; k++) {
        uint16_t value = block[k];
        if (value < lo) lo = value;
        if (value > hi) hi = value;
        total += value;
    }
    uart_string(name);
    uart_string(" raw: min "); uart_int32(lo);
    uart_string(" max "); uart_int32(hi);
    uart_string(" avg "); uart_int32(total / samples); // total divided by count is the average
    uart_string(" span "); uart_int32(hi - lo); // highest minus lowest is the peak-to-peak span
    uart_string("\r\n");
}

// Print the saved ADC state to help find why a capture did not finish.
// s points to the saved struct. s->channel means its channel field.
void uart_send_timeout_status(const adc_status_t *s) {
    // ch: channel, pos: samples stored, armed: waiting for an edge, active: sampling now.
    // zc: edges counted; mux and ctrl: saved ADMUX and ADCSRA register values.
    uart_string("ADC timeout: ch="); uart_int32(s->channel);
    uart_string(" pos="); uart_int32(s->position);
    uart_string(" armed="); uart_int32(s->armed);
    uart_string(" active="); uart_int32(s->capturing);
    uart_string(" zc="); uart_int32(s->zero_crossings);
    uart_string(" mux="); uart_int32(s->mux);
    uart_string(" ctrl="); uart_int32(s->control);
    uart_string("\r\n");
}

// Print both channels plus the precise RMS current, after the capture has finished.
void uart_send_capture_diagnostics(uint32_t current_mA_x100) {
    send_raw_stats("V", v_samples);
    send_raw_stats("I", i_samples);
    uart_string("I rms_mA_x100: "); uart_int32(current_mA_x100);
    uart_string("\r\n");
}
