/*
 * JouleObserverFinal.c
 *
 * Created: 20/10/2026 11:40:57 am
 * Author : Admin
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "adc.h"
#include "int0.h"
#include "computation.h"
#include "display.h"
#include "uart.h"

#define DISPLAY_UPDATE_TICKS (TIMER1_TICKS_PER_SECOND / 4UL)
#define UART_PERIOD_TICKS    TIMER1_TICKS_PER_SECOND
#define NO_SIGNAL_TICKS      (TIMER1_TICKS_PER_SECOND / 2UL)

int main(void) {
    uint32_t display_ticks = DISPLAY_UPDATE_TICKS;
    uint32_t uart_ticks = UART_PERIOD_TICKS;
    uint32_t mode_ticks = 0;
    uint32_t last_ticks;
    uint8_t last_mode = 255;
    uint32_t capture_started;
    uint8_t raw_due = 1;
    uint32_t current_mA_x100 = 0;
    uint32_t vrms_mV = 0, current_mA = 0, peak_mA = 0, power_mW = 0;
    uint32_t apparent_mVA = 0, frequency_mHz = 0;
    int16_t power_factor_x1000 = -1;

    display_init();
    uart_init();
    calcs_init();
    int0_init();
    adc_init();
    sei();
    uart_string("BOOT JouleObserver LIVE: ADC0=V ADC1=I VZC=INT0\r\n");
    adc_start_capture();
    last_ticks = calcs_ticks();
    capture_started = last_ticks;

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

        if (adc_capture_done()) {
            vrms_mV = calc_voltage_rms_mV();
            current_mA_x100 = calc_current_rms_mA_x100();
            current_mA = (current_mA_x100 + 50) / 100;
            peak_mA = calc_current_peak_mA();
            power_mW = calc_power_mW();
            apparent_mVA = calc_apparent_power_mVA(vrms_mV, current_mA_x100);
            power_factor_x1000 = calc_power_factor_x1000(power_mW, apparent_mVA);
            frequency_mHz = calc_frequency_mHz();
            // Publish fresh values before the long waveform UART transfer.
            display_update(vrms_mV, peak_mA, power_mW, energy_mWh());
            if (raw_due) {
                // Read only while the buffers are frozen.
                uart_send_capture_diagnostics(current_mA_x100);
                // Buffers stay frozen until the complete waveform frame is sent.
                uart_send_waveform();
                uart_send_calibration();
                raw_due = 0;
            }
            adc_start_capture();
            capture_started = calcs_ticks();
        } else if ((uint32_t)(calcs_ticks() - capture_started) >= NO_SIGNAL_TICKS) {
            adc_status_t status;
            adc_get_status(&status);
            vrms_mV = current_mA = peak_mA = power_mW = apparent_mVA = frequency_mHz = 0;
            power_factor_x1000 = -1;
            adc_start_capture();  // atomically abort partial captures before rearming
            capture_started = calcs_ticks();
            uart_send_timeout_status(&status);
        }
        if (display_ticks >= DISPLAY_UPDATE_TICKS) {
            display_ticks %= DISPLAY_UPDATE_TICKS;
            display_update(vrms_mV, peak_mA, power_mW, energy_mWh());
        }
        if (uart_ticks >= UART_PERIOD_TICKS) {
            uart_ticks %= UART_PERIOD_TICKS;
            uart_send_measurements(vrms_mV, current_mA, power_mW, energy_mWh(), peak_mA, apparent_mVA, power_factor_x1000, frequency_mHz);
            raw_due = 1;
        }
    }
}
