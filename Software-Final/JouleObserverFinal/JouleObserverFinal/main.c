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

// Timer1 counts time in ticks. Dividing ticks per second by 4 gives a 250 ms interval.
// These intervals refresh the values; display.c handles the one-second scrolling itself.
#define DISPLAY_UPDATE_TICKS (TIMER1_TICKS_PER_SECOND / 4UL)
#define UART_PERIOD_TICKS    TIMER1_TICKS_PER_SECOND
// If the two ADC blocks have not finished after half a second, try again.
#define NO_SIGNAL_TICKS      (TIMER1_TICKS_PER_SECOND / 2UL)

// Program entry point: set up the hardware once, then keep checking for work.
int main(void) {
    // Count elapsed time separately for display updates, measurements and mode messages.
    // Starting the first two at their limits makes the first update happen immediately.
    uint32_t display_ticks = DISPLAY_UPDATE_TICKS;
    uint32_t uart_ticks = UART_PERIOD_TICKS;
    uint32_t mode_ticks = 0;
    uint32_t last_ticks;
    uint8_t last_mode = 255; // outside the valid modes, so the first mode is always sent
    uint32_t capture_started;
    uint8_t raw_due = 1;     // 1 = send a waveform/calibration report after the next capture
    uint32_t current_mA_x100 = 0; // keep hundredths of a mA until the final UART rounding
    // Keep the most recent readings here between captures. All start at zero.
    uint32_t vrms_mV = 0, current_mA = 0, peak_mA = 0, power_mW = 0;
    uint32_t apparent_mVA = 0, frequency_mHz = 0;
    int16_t power_factor_x1000 = -1; // -1 means no valid power factor yet

    // Each init function sets up one part of the hardware before interrupts are enabled.
    display_init();
    uart_init();
    calcs_init();
    int0_init();
    adc_init();
    sei(); // allow enabled interrupts to run, including display scanning and ADC results
    uart_string("BOOT JouleObserver LIVE: ADC0=V ADC1=I VZC=INT0\r\n");
    adc_start_capture(); // prepare the ADC; the next voltage zero crossing starts sampling
    last_ticks = calcs_ticks();
    capture_started = last_ticks;

    // Run forever. Interrupts collect the samples while this loop handles the results.
    while (1) {
        uint32_t now = calcs_ticks();
        // dt is the time since the previous loop, not the total time since power-on.
        // Unsigned subtraction also works when the 32-bit timer count wraps back to zero.
        uint32_t dt = (uint32_t)(now - last_ticks);
        last_ticks = now;
        // Poll continuously, including time spent sending UART. Timer1 wraps safely.
        energy_add(power_mW, dt); // add energy using the last measured power and elapsed time
        display_ticks += dt;
        uart_ticks += dt;
        mode_ticks += dt;

        uint8_t mode = display_current_quantity(); // which measurement the local display shows
        // Send on every change and repeat every 250 ms so ESP32 can restart alone.
        if (mode != last_mode || mode_ticks >= DISPLAY_UPDATE_TICKS) {
            uart_send_mode(mode);
            last_mode = mode;
            mode_ticks = 0;
        }

        // Both 200-sample arrays are complete and remain unchanged until we rearm the ADC.
        if (adc_capture_done()) {
            vrms_mV = calc_voltage_rms_mV();
            current_mA_x100 = calc_current_rms_mA_x100();
            current_mA = (current_mA_x100 + 50) / 100; // round hundredths to whole mA for UART
            peak_mA = calc_current_peak_mA();
            power_mW = calc_power_mW();
            // Apparent power uses the more precise current value, before whole-mA rounding.
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
                raw_due = 0; // report sent; wait for the next UART interval before another
            }
            adc_start_capture(); // finished reading the arrays, so they can be filled again
            // Start timeout timing after the UART transfer, which can take several seconds.
            capture_started = calcs_ticks();
        } else if ((uint32_t)(calcs_ticks() - capture_started) >= NO_SIGNAL_TICKS) {
            // Capture took too long: save its status before resetting it, then show zero readings.
            adc_status_t status;
            adc_get_status(&status); // copy the ADC state so it can be printed after the restart
            vrms_mV = current_mA = peak_mA = power_mW = apparent_mVA = frequency_mHz = 0;
            power_factor_x1000 = -1;
            adc_start_capture();  // atomically abort partial captures before rearming
            capture_started = calcs_ticks();
            uart_send_timeout_status(&status);
        }
        // Refresh the local display values once at least 250 ms has elapsed.
        if (display_ticks >= DISPLAY_UPDATE_TICKS) {
            // %= keeps the leftover ticks instead of throwing away part of the next interval.
            display_ticks %= DISPLAY_UPDATE_TICKS;
            display_update(vrms_mV, peak_mA, power_mW, energy_mWh());
        }
        // Send a short measurement packet when at least one second has elapsed.
        // Longer waveform transfers can delay this; UART is not sent from an interrupt.
        if (uart_ticks >= UART_PERIOD_TICKS) {
            uart_ticks %= UART_PERIOD_TICKS;
            uart_send_measurements(vrms_mV, current_mA, power_mW, energy_mWh(), peak_mA, apparent_mVA, power_factor_x1000, frequency_mHz);
            raw_due = 1; // request a waveform and calibration report from the next completed capture
        }
    }
}
