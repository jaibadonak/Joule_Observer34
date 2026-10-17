/*
 * Project_testing.c
 *
 * Created: 17/10/2026 12:30:05 pm
 * Author : Admin
 *

#include <avr/io.h>


int main(void)
{
    /* Replace with your application code *
    while (1) 
    {
    }
}
*/
	
	/*
 * main.c  -  ADC TEST VERSION
 *
 * Tests only the ADC capture: zero crossing (INT0) -> V block -> I block.
 * After every capture it sends the raw samples over UART (9600 8N1) as
 * "v,i" lines in ADC counts, so you can read them in a terminal or paste
 * them into a spreadsheet and plot them.
 *
 * Needs in the project: main.c, adc.c/h, vzc.c, interrupts.h, uart.c/h
 * (display.c and calcs.c can stay in the project, this test doesn't use them)
 *
 * What you should see (14 Vrms in, 500 Hz)
 *   CAPTURE 1  (v,i in ADC counts)
 *   v,i pairs, 200 lines: both columns swing like a sine around about 422
 *   (V from about 240 to 600, I from about 230 to 620 at full load), about 19 lines per cycle
 *   END
 *   then CAPTURE 2, 3, ... (about every 2 s, printing the lines takes most of that)
 *
 * If something is wrong
 *   NO ZERO CROSSING   INT0 (PD2) never sees a rising edge: check the comparator output
 *   all values 0 or 1023, or stuck on one number   nothing sensible on PC0 / PC1
 *   values swing but I and V are the same shape    channels swapped or one input missing
 *   nothing printed at all   UART/TXD wiring, or the program did not load
 *

#define F_CPU 2000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "adc.h"
#include "interrupts.h"
#include "uart.h"

int main(void)
{
	uint16_t k;
	uint16_t waited;
	uint8_t  capture_number = 0;

	uart_init();
	int0_init();
	adc_init();
	sei();

	while (1)
	{
		adc_start_capture();
		//uart_send_measurements((int32_t)vrms_mV, (int32_t)ipk_mA, (int32_t)power_mW, (int32_t)energy_mWh());

		// wait up to 1 s for the V and I blocks to finish
		waited = 0;
		while (!adc_capture_done() && waited < 1000) {
			_delay_ms(1);
			waited++;
		}

		if (!adc_capture_done()) {
			uart_string("NO ZERO CROSSING (check INT0 / PD2)\r\n");
			continue;
		}

		capture_number++;
		uart_string("CAPTURE ");
		uart_int32(capture_number);
		uart_string("  (v,i in ADC counts)\r\n");

		for (k = 0; k < samples; k++) {
			uart_int32(v_samples[k]);
			uart_transmit(',');
			uart_int32(i_samples[k]);
			uart_string("\r\n");
		}
		uart_string("END\r\n");
	} 
}
*/
	
/*
 * main.c  -  TERMINAL REPORT VERSION
 *
 * Reads V and I through the ADC, works out the RMS voltage, RMS current,
 * real power and energy, and prints them over UART (9600 8N1) once a second.
 * Open the Proteus Virtual Terminal on TXD and you should see, every second:
 *
 *     Voltage: 14.04V
 *     Current: 535.39mA
 *     Power: 7.54W
 *     Energy: 0.021Wh
 *
 * Needs in the project: main.c, adc.c/h, computation.c/h, vzc.c, interrupts.h, uart.c/h, common.h
 * (display.c can stay in the project, this version doesn't start the display)
 *
 * If something is wrong
 *   Nothing printed         program not loaded, TXD / terminal baud wrong (9600)
 *   Everything stays 0.00   no zero crossing reaching INT0 (PD2), or no signal on PC0 / PC1
 *   Numbers wrong by a constant factor   check the scaling in computation.h
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "adc.h"
#include "computation.h"
#include "interrupts.h"
#include "uart.h"

// Times are counted in Timer1 ticks (32 us each, see calcs_init)
#define PRINT_PERIOD_TICKS   31250      // print every 1 s
#define NO_SIGNAL_TICKS      15625      // no finished capture for 0.5 s = no signal, report zeros

// Print value / 10^decimals, e.g. print_fixed(1404, 2) prints 14.04
static void print_fixed(uint32_t value, uint8_t decimals)
{
	uint32_t divisor = 1;
	for (uint8_t i = 0; i < decimals; i++) {
		divisor = divisor * 10;
	}

	uart_int32((int32_t)(value / divisor));         // whole part
	uart_transmit('.');

	uint32_t fraction = value % divisor;
	divisor = divisor / 10;
	while (divisor > 0) {                           // one digit at a time, keeps the leading zeros
		uart_transmit('0' + (char)((fraction / divisor) % 10));
		divisor = divisor / 10;
	}
}

int main(void)
{
	uint16_t last_ticks;                // Timer1 count when the last result came in
	uint16_t now_ticks;
	uint16_t dt_ticks;                  // time since the last result
	uint16_t print_ticks = PRINT_PERIOD_TICKS;      // start full so the first result prints at once

	uint32_t vrms_mV      = 0;
	uint32_t irms_mA_x100 = 0;          // hundredths of a mA
	uint32_t power_mW     = 0;
	uint8_t  new_result;

	uart_init();
	calcs_init();                       // starts Timer1
	int0_init();
	adc_init();
	sei();

	adc_start_capture();
	last_ticks = TCNT1;

	while (1)
	{
		now_ticks = TCNT1;
		dt_ticks = (uint16_t)(now_ticks - last_ticks);
		new_result = 0;

		if (adc_capture_done()) {
			// v_samples[] and i_samples[] are full and stay untouched until the next capture starts
			vrms_mV      = calc_voltage_rms_mV();
			irms_mA_x100 = calc_current_rms_mA_x100();
			power_mW     = calc_power_mW();
			new_result = 1;
		}
		else if (dt_ticks > NO_SIGNAL_TICKS) {
			// no zero crossings arriving, so no signal
			vrms_mV = 0;
			irms_mA_x100 = 0;
			power_mW = 0;
			new_result = 1;
		}

		if (new_result) {
			last_ticks = now_ticks;
			adc_start_capture();                    // results are copied out, start the next capture now
			energy_add(power_mW, dt_ticks);

			print_ticks = print_ticks + dt_ticks;
			if (print_ticks >= PRINT_PERIOD_TICKS) {
				print_ticks = print_ticks - PRINT_PERIOD_TICKS;

				uart_string("Voltage: ");
				print_fixed((vrms_mV + 5) / 10, 2);             // mV -> V, 2 decimals
				uart_string("V\r\n");

				uart_string("Current: ");
				print_fixed(irms_mA_x100, 2);                   // mA, 2 decimals
				uart_string("mA\r\n");

				uart_string("Power: ");
				print_fixed((power_mW + 5) / 10, 2);            // mW -> W, 2 decimals
				uart_string("W\r\n");

				uart_string("Energy: ");
				print_fixed(energy_mWh(), 3);                   // mWh -> Wh, 3 decimals
				uart_string("Wh\r\n\r\n");
			}
		}
	}
}