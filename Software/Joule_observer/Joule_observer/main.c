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
 *

#define F_CPU 2000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "display.h"
#include "uart.h"

#include "adc.h"            // TEST 3: uncomment when adc.c is finished

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
	adc_init();                       // TEST 3: uncomment (before sei)
	sei();                              // the display scan interrupt needs this

	adc_start_capture();              // TEST 3: uncomment

	while (1)
	{
		//---------------- TEST 1 and 2: display + UART with made-up numbers ----------------//
		/*volts_mV   = 14000 + (int32_t)(tick % 20) * 10;     // 14.00 ... 14.19 V
		current_mA = 400   + (int32_t)(tick % 10) * 5;      // 400 ... 445 mA
		power_mW   = 5600  + (int32_t)(tick % 10) * 20;     // 5.60 ... 5.78 W

		energy_mWs = energy_mWs + power_mW;                 // power x 1 second
		energy_mWh = energy_mWs / 3600;                     // 3600 seconds in an hour

		display_update(volts_mV, current_mA, power_mW, energy_mWh);
		uart_send_measurements(volts_mV, current_mA, power_mW);*/

		//---------------- TEST 3: ADC samples sent over UART (uncomment when ready) ----------------//
		// Rename the calls to match your adc.c. This prints the first 5 raw samples //
		// of each capture as "v i" pairs, so you can check them in the terminal. //
		
/*		 if (adc_capture_done()) {
		     for (uint8_t k = 0; k < 5; k++) {
		         uart_int32(v_samples[k]);
		         uart_transmit(' ');
		         uart_int32(i_samples[k]);
		         uart_string("\r\n");
		     }
		     adc_start_capture();
		 }

		//tick++;
		//_delay_ms(1000);
	}
}
*/

/*
 * Joule_observer.c
 *
 * Created: 14/10/2026 5:04:55 pm
 * Author : Admin
 *
 * Joule Observer: reads the voltage and current waveforms through the ADC,
 * works out Vrms, peak current, real power and energy, shows them on the
 * display and sends them over UART.
 *
 *   zero crossing (INT0) -> ADC capture (V block, then I block) -> calcs
 *       -> display_update (4 times a second) and uart_send_measurements (once a second)
 *
 * The project needs: main.c, adc.c/h, calcs.c/h, display.c/h, uart.c/h,
 * vzc.c, interrupts.h, common.h
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "adc.h"
#include "calcs.h"
#include "display.h"
#include "interrupts.h"
#include "uart.h"

// Times are counted in Timer1 ticks (32 us each, see calcs_init)
#define DISPLAY_UPDATE_TICKS   7812     // refresh the numbers on the display every 0.25 s
#define UART_PERIOD_TICKS      31250    // send a UART line every 1 s
#define NO_SIGNAL_TICKS        15625    // no finished capture for 0.5 s = no signal, show zeros

int main(void)
{
	uint16_t last_ticks;                // Timer1 count when the last result came in
	uint16_t now_ticks;
	uint16_t dt_ticks;                  // time since the last result
	uint16_t display_ticks = DISPLAY_UPDATE_TICKS;   // start full so the first result shows at once
	uint16_t uart_ticks    = UART_PERIOD_TICKS;      // same for the first UART line

	uint32_t vrms_mV  = 0;
	uint32_t ipk_mA   = 0;
	uint32_t power_mW = 0;
	uint8_t  new_result;

	display_init();
	uart_init();
	calcs_init();                       // starts Timer1
	int0_init();
	adc_init();
	sei();                              // display scan, INT0 and ADC interrupts all need this

	adc_start_capture();
	last_ticks = TCNT1;

	while (1)
	{
		now_ticks = TCNT1;
		dt_ticks = (uint16_t)(now_ticks - last_ticks);
		new_result = 0;

		if (adc_capture_done()) {
			// v_samples[] and i_samples[] are full and stay untouched until the next capture starts
			vrms_mV  = calc_voltage_rms_mV();
			ipk_mA   = calc_current_peak_mA();
			power_mW = calc_power_mW();
			new_result = 1;
		}
		else if (dt_ticks > NO_SIGNAL_TICKS) {
			// no zero crossings arriving, so no signal
			vrms_mV  = 0;
			ipk_mA   = 0;
			power_mW = 0;
			new_result = 1;
		}

		if (new_result) {
			last_ticks = now_ticks;
			adc_start_capture();                    // results are copied out, start the next capture now

			energy_add(power_mW, dt_ticks);

			display_ticks = display_ticks + dt_ticks;
			if (display_ticks >= DISPLAY_UPDATE_TICKS) {
				display_ticks = 0;
				display_update((int32_t)vrms_mV, (int32_t)ipk_mA, (int32_t)power_mW, (int32_t)energy_mWh());
			}

			uart_ticks = uart_ticks + dt_ticks;
			if (uart_ticks >= UART_PERIOD_TICKS) {
				uart_ticks = 0;
				uart_send_measurements((int32_t)vrms_mV, (int32_t)ipk_mA, (int32_t)power_mW);
			}
		}
	}
}