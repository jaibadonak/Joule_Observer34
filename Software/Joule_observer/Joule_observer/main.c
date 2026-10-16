/*
 * main.c  -  TEST VERSION
 *
 * Tests the display and the UART together using made-up measurements, so
 * nothing else in the project (adc, calcs, zero crossings) is needed.
 *
 * For this test the project should contain ONLY:
 *     main.c, display.c, display.h, uart.c, uart.h
 * Take adc.c, calcs.c, vzc.c, izc.c out of the project until they build
 * (right-click > Exclude From Project, the files stay on disk).
 *
 * What you should see
 *   Display  scrolls every 3 s:  14.00 V -> 400 mA -> 5.60 W -> 0.014 Wh
 *            the numbers tick up a little every second
 *   UART     one line a second at 9600 8N1, e.g.   $M,14000,400,5600
 *            (open a serial terminal on the USB-serial port to read it)
 */

#define F_CPU 2000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "display.h"
#include "uart.h"

//#include "adc.h"            // TEST 3: uncomment when adc.c is finished

int main(void)
{
	uint16_t tick = 0;                  // counts seconds
	int32_t volts_mV;
	int32_t current_mA;
	int32_t power_mW;
	int32_t energy_mWs = 0;             // energy so far in milliwatt-seconds
	int32_t energy_mWh;

	display_init();
	uart_init();
	//adc_init();                       // TEST 3: uncomment (before sei)
	sei();                              // the display scan interrupt needs this

	//adc_start_capture();              // TEST 3: uncomment

	while (1)
	{
		//---------------- TEST 1 and 2: display + UART with made-up numbers ----------------//
		volts_mV   = 14000 + (int32_t)(tick % 20) * 10;     // 14.00 ... 14.19 V
		current_mA = 400   + (int32_t)(tick % 10) * 5;      // 400 ... 445 mA
		power_mW   = 5600  + (int32_t)(tick % 10) * 20;     // 5.60 ... 5.78 W

		energy_mWs = energy_mWs + power_mW;                 // power x 1 second
		energy_mWh = energy_mWs / 3600;                     // 3600 seconds in an hour

		display_update(volts_mV, current_mA, power_mW, energy_mWh);
		uart_send_measurements(volts_mV, current_mA, power_mW);

		//---------------- TEST 3: ADC samples sent over UART (uncomment when ready) ----------------//
		// Rename the calls to match your adc.c. This prints the first 5 raw samples
		// of each capture as "v i" pairs, so you can check them in the terminal.
		//
		// if (adc_capture_done()) {
		//     for (uint8_t k = 0; k < 5; k++) {
		//         uart_int32(v_samples[k]);
		//         uart_transmit(' ');
		//         uart_int32(i_samples[k]);
		//         uart_string("\r\n");
		//     }
		//     adc_start_capture();
		// }

		tick++;
		_delay_ms(1000);
	}
}