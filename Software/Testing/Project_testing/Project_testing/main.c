/*
 * main.c  -  FULL PIPELINE: ADC -> calculations -> display + UART (ESP32)
 * 2 MHz system clock.
 *
 *   zero crossing (INT0) -> ADC capture (V block, then I block) -> calcs
 *       -> display_update (4 times a second) and uart_send_measurements (once a second)
 *
 * The calc functions are called ONCE per finished capture, never inside the
 * display / UART calls. Their answers are kept in variables.
 *
 * Needs in the project: main.c, adc.c/h, computation.c/h, display.c/h,
 * vzc_interrupt.c, interrupts.h, uart.c/h
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "adc.h"
#include "computation.h"
#include "display.h"
#include "interrupts.h"
#include "uart.h"

// Timer1 is clk/64 = 31250 Hz at 2 MHz, so 1 tick = 32 us
#define DISPLAY_UPDATE_TICKS   7812     // refresh the display numbers every 0.25 s
#define SEND_PERIOD_TICKS      31250    // one UART line every 1 s
#define NO_SIGNAL_TICKS        15625    // no finished capture for 0.5 s = no signal, show zeros

// DEBUG: send min / max / average of one raw ADC block
static void send_raw_stats(const char *name, volatile uint16_t *block)
{
	uint16_t lo = 1023, hi = 0;
	uint32_t total = 0;
	for (uint16_t k = 0; k < samples; k++) {
		if (block[k] < lo) lo = block[k];
		if (block[k] > hi) hi = block[k];
		total = total + block[k];
	}
	uart_string(name);
	uart_string(" raw: min ");  uart_int32(lo);
	uart_string(" max ");       uart_int32(hi);
	uart_string(" avg ");       uart_int32((int32_t)(total / samples));
	uart_string("\r\n");
}

int main(void)
{
	uint16_t last_ticks;
	uint16_t now_ticks;
	uint16_t dt_ticks;
	uint16_t display_ticks = DISPLAY_UPDATE_TICKS;   // start full so the first result shows at once
	uint16_t send_ticks    = SEND_PERIOD_TICKS;

	uint32_t vrms_mV      = 0;
	uint32_t irms_mA_x100 = 0;
	uint32_t irms_mA      = 0;
	uint32_t power_mW     = 0;
	uint8_t  new_result;

	display_init();
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
			// both blocks are full and stay untouched until the next capture starts
			vrms_mV      = calc_voltage_rms_mV();
			irms_mA_x100 = calc_current_rms_mA_x100();
			irms_mA      = (irms_mA_x100 + 50) / 100;
			power_mW     = calc_power_mW();
			new_result = 1;
		}
		else if (dt_ticks > NO_SIGNAL_TICKS) {
			vrms_mV = 0;
			irms_mA = 0;
			power_mW = 0;
			new_result = 1;
		}

		if (new_result) {
			last_ticks = now_ticks;
			adc_start_capture();                    // results are copied out, start the next capture
			energy_add(power_mW, dt_ticks);

			display_ticks = display_ticks + dt_ticks;
			if (display_ticks >= DISPLAY_UPDATE_TICKS) {
				display_ticks = 0;
				display_update((int32_t)vrms_mV, (int32_t)irms_mA, (int32_t)power_mW, (int32_t)energy_mWh());
			}

			send_ticks = send_ticks + dt_ticks;
			if (send_ticks >= SEND_PERIOD_TICKS) {
				send_ticks = send_ticks - SEND_PERIOD_TICKS;
				uart_send_measurements((int32_t)vrms_mV, (int32_t)irms_mA, (int32_t)power_mW);
				send_raw_stats("V", v_samples);          // DEBUG: remove when working
				send_raw_stats("I", i_samples);
			}
		}
	}
}