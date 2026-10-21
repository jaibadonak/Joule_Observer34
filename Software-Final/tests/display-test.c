// Separate entry point for the fixed display test. No ADC input is needed.
#include <avr/interrupt.h>
#include "computation.h"
#include "display.h"
#include "uart.h"

// Alternative program entry point, used only when building display-test.hex.
int main(void) {
    // Start the display scan, UART and timer. ADC and zero crossing input are not needed here.
    display_init();
    uart_init();
    calcs_init();
    display_update(13700, 566, 5480, 123); // 13.7 V RMS, 566 mA peak, 5.48 W, 123 mWh
    sei(); // let the display timer interrupt scan the digits
    uart_string("BOOT JouleObserver DISPLAY_SELF_TEST: fixed readings\r\n");

    // Remember when each type of UART message was last sent.
    uint32_t last_reading = calcs_ticks();
    uint32_t last_mode_report = last_reading;
    uint8_t last_mode = 255; // an invalid mode, which makes the first valid mode get reported
    // Keep the fixed display running and report its mode and test readings repeatedly.
    while (1) {
        uint32_t now = calcs_ticks();
        uint8_t mode = display_current_quantity();
        // Report mode changes immediately, and repeat after 250 ms if it has not changed.
        if (mode != last_mode || now - last_mode_report >= TIMER1_TICKS_PER_SECOND / 4UL) {
            uart_send_mode(mode);
            last_mode = mode;
            last_mode_report = now;
        }
        // Once per second send the same known readings, so UART can be checked too.
        if (now - last_reading >= TIMER1_TICKS_PER_SECOND) {
            // V mV, I RMS mA, P mW, energy mWh, I peak mA, S mVA, PF x1000, frequency mHz.
            uart_send_measurements(13700, 400, 5480, 123, 566, 5480, 1000, 500000);
            last_reading = now;
        }
    }
}
