// Separate entry point for the fixed display test. No ADC input is needed.
#include <avr/interrupt.h>
#include "computation.h"
#include "display.h"
#include "uart.h"

int main(void) {
    display_init();
    uart_init();
    calcs_init();
    display_update(13700, 566, 5480, 123);
    sei();
    uart_string("BOOT JouleObserver DISPLAY_SELF_TEST: fixed readings\r\n");

    uint32_t last_reading = calcs_ticks();
    uint32_t last_mode_report = last_reading;
    uint8_t last_mode = 255;
    while (1) {
        uint32_t now = calcs_ticks();
        uint8_t mode = display_current_quantity();
        if (mode != last_mode || now - last_mode_report >= TIMER1_TICKS_PER_SECOND / 4UL) {
            uart_send_mode(mode);
            last_mode = mode;
            last_mode_report = now;
        }
        if (now - last_reading >= TIMER1_TICKS_PER_SECOND) {
            uart_send_measurements(13700, 400, 5480, 123, 566, 5480, 1000, 500000);
            last_reading = now;
        }
    }
}
