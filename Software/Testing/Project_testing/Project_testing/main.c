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
 */

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
