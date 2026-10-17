/*
 * main.c  -  ESP32 + TERMINAL VERSION
 *
 * Reads V and I through the ADC, works out the RMS voltage, RMS current,
 * real power and energy, and once a second sends over UART (9600 8N1, TXD = PD1):
 *
 *   1. one line for the ESP32:        $M,14050,535,6030,4
 *                                      (Vrms mV, Irms mA, power mW, energy mWh)
 *   2. readable text for a terminal:   Voltage: 14.05V
 *                                      Current: 535.02mA
 *                                      Power: 6.03W
 *                                      Energy: 0.004Wh
 *
 * The ESP32 only reads lines that start with $M or $W and ignores everything
 * else, so both can go out on the same wire. Remove the terminal part if you
 * want only the $M line.
 *
 * Needs in the project: main.c, adc.c/h, computation.c/h, vzc.c, interrupts.h, uart.c/h, common.h
 * (display.c can stay in the project, this version doesn't start the display)
 */

#define F_CPU 20000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "adc.h"
#include "computation.h"
#include "interrupts.h"
#include "uart.h"

// Times are counted in Timer1 ticks (32 us each, see calcs_init)
#define SEND_PERIOD_TICKS    31250      // send every 1 s
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

// The line the ESP32 reads: $M,<Vrms mV>,<Irms mA>,<power mW>,<energy mWh>
static void send_esp_line(uint32_t vrms_mV, uint32_t irms_mA, uint32_t power_mW, uint32_t energy_mWh)
{
	uart_string("$M,");
	uart_int32((int32_t)vrms_mV);
	uart_transmit(',');
	uart_int32((int32_t)irms_mA);
	uart_transmit(',');
	uart_int32((int32_t)power_mW);
	uart_transmit(',');
	uart_int32((int32_t)energy_mWh);
	uart_string("\r\n");
}

int main(void)
{
	uint16_t last_ticks;                // Timer1 count when the last result came in
	uint16_t now_ticks;
	uint16_t dt_ticks;                  // time since the last result
	uint16_t send_ticks = SEND_PERIOD_TICKS;        // start full so the first result goes out at once

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

			send_ticks = send_ticks + dt_ticks;
			if (send_ticks >= SEND_PERIOD_TICKS) {
				send_ticks = send_ticks - SEND_PERIOD_TICKS;

				// 1. for the ESP32 (whole mA, rounded)
				send_esp_line(vrms_mV, (irms_mA_x100 + 50) / 100, power_mW, energy_mWh());

				// 2. for the terminal
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