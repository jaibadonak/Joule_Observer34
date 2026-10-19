/*
 * 2 MHz ATmega328PB: V/I capture -> calculations -> display + UART.
 * Build with -DDISPLAY_SELF_TEST for fixed 13.7U, 566A (mA peak), 5.48P.
 * In that build no analogue inputs or zero crossings are needed.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "adc.h"
#include "computation.h"
#include "display.h"
#include "interrupts.h"
#include "uart.h"

#define DISPLAY_UPDATE_TICKS (TIMER1_TICKS_PER_SECOND / 4UL)
#define UART_PERIOD_TICKS    TIMER1_TICKS_PER_SECOND
#define NO_SIGNAL_TICKS      (TIMER1_TICKS_PER_SECOND / 2UL)

#ifndef DISPLAY_SELF_TEST
static void send_raw_stats(const char *name, volatile uint16_t *block) {
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
    uart_string(" avg "); uart_int32(total / samples);
    uart_string(" span "); uart_int32(hi - lo);
    uart_string("\r\n");
}

static void send_timeout_status(const adc_status_t *s) {
    uart_string("ADC timeout: ch="); uart_int32(s->channel);
    uart_string(" pos="); uart_int32(s->position);
    uart_string(" armed="); uart_int32(s->armed);
    uart_string(" active="); uart_int32(s->capturing);
    uart_string(" zc="); uart_int32(s->zero_crossings);
    uart_string(" mux="); uart_int32(s->mux);
    uart_string(" ctrl="); uart_int32(s->control);
    uart_string("\r\n");
}
#endif

int main(void) {
    uint32_t display_ticks = DISPLAY_UPDATE_TICKS;
    uint32_t uart_ticks = UART_PERIOD_TICKS;
    uint32_t mode_ticks = 0;
    uint32_t last_ticks;
    uint8_t last_mode = 255;
#ifndef DISPLAY_SELF_TEST
    uint32_t capture_started;
    uint8_t raw_due = 1;
    uint32_t current_mA_x100 = 0;
#endif
    uint32_t vrms_mV = 0, current_mA = 0, peak_mA = 0, power_mW = 0;

    display_init();
    uart_init();
    calcs_init();
#ifndef DISPLAY_SELF_TEST
    int0_init();
    adc_init();
#endif
    sei();
#ifndef DISPLAY_SELF_TEST
    uart_string("BOOT JouleObserver LIVE: ADC0=V ADC1=I VZC=INT0\r\n");
    adc_start_capture();
#else
    uart_string("BOOT JouleObserver DISPLAY_SELF_TEST: fixed readings\r\n");
#endif
    last_ticks = calcs_ticks();
#ifndef DISPLAY_SELF_TEST
    capture_started = last_ticks;
#endif

    while (1) {
        uint32_t now = calcs_ticks();
        uint32_t dt = (uint32_t)(now - last_ticks);
        last_ticks = now;
        // Poll continuously, including time spent sending UART. Timer1 wraps safely.
        energy_add(power_mW, dt);
        display_ticks += dt;
        uart_ticks += dt;
        mode_ticks += dt;

        uint8_t mode = display_current_quantity();
        // Send on every change and repeat every 250 ms so ESP32 can restart alone.
        if (mode != last_mode || mode_ticks >= DISPLAY_UPDATE_TICKS) {
            uart_send_mode(mode);
            last_mode = mode;
            mode_ticks = 0;
        }

#ifdef DISPLAY_SELF_TEST
        vrms_mV = 13700;
        current_mA = 400;
        peak_mA = 566;
        power_mW = 5480;
#else
        if (adc_capture_done()) {
            vrms_mV = calc_voltage_rms_mV();
            current_mA_x100 = calc_current_rms_mA_x100();
            current_mA = (current_mA_x100 + 50) / 100;
            peak_mA = calc_current_peak_mA();
            power_mW = calc_power_mW();
            // Publish fresh values before the long raw-waveform UART transfer.
            display_update(vrms_mV, peak_mA, power_mW, energy_mWh());
            if (raw_due) {
                // Read only while the buffers are frozen.
                send_raw_stats("V", v_samples);
                send_raw_stats("I", i_samples);
                uart_string("I rms_mA_x100: "); uart_int32(current_mA_x100);
                uart_string("\r\n");
                // Buffers stay frozen until the complete waveform line is sent.
                uart_send_waveform();
                uart_send_calibration();
                raw_due = 0;
            }
            adc_start_capture();
            capture_started = calcs_ticks();
        } else if ((uint32_t)(calcs_ticks() - capture_started) >= NO_SIGNAL_TICKS) {
            adc_status_t status;
            adc_get_status(&status);
            vrms_mV = current_mA = peak_mA = power_mW = 0;
            adc_start_capture();  // atomically abort partial captures before rearming
            capture_started = calcs_ticks();
            send_timeout_status(&status);
        }
#endif
        if (display_ticks >= DISPLAY_UPDATE_TICKS) {
            display_ticks %= DISPLAY_UPDATE_TICKS;
#ifdef DISPLAY_SELF_TEST
            display_update(vrms_mV, peak_mA, power_mW, 123);
#else
            display_update(vrms_mV, peak_mA, power_mW, energy_mWh());
#endif
        }
        if (uart_ticks >= UART_PERIOD_TICKS) {
            uart_ticks %= UART_PERIOD_TICKS;
#ifdef DISPLAY_SELF_TEST
            uart_send_measurements(vrms_mV, current_mA, power_mW, 123, peak_mA);
#else
            uart_send_measurements(vrms_mV, current_mA, power_mW, energy_mWh(), peak_mA);
#endif
#ifndef DISPLAY_SELF_TEST
            raw_due = 1;
#endif
        }
    }
}
