/*
 * display_test.c
 *
 * Stand-alone test run for display.c / display.h. The rest of the project
 * (adc, calcs, ...) does not build yet, so this has its own main().
 *
 * How to run it
 *   1. New Atmel Studio project (GCC C Executable, ATmega328P), or a copy of the
 *      project folder, so the real main.c is left alone.
 *   2. Add display.c, display.h and this file (right-click project > Add >
 *      Existing Item). Only ONE file may contain main().
 *   3. Build (Debug is fine, _delay_ms needs optimisation on) and run it on the
 *      board or in Proteus.
 *
 * What you should see
 *   Test 1, 12 s of fixed numbers. This checks the wiring and the digit order:
 *       3 s   12.34      (voltage)
 *       3 s   5678       (current, right-aligned)
 *       3 s   90.10      (power)
 *       3 s   1.234      (energy)
 *   Test 2, numbers that change once a second. This checks that updates work:
 *       voltage counts up 14.00, 14.01, 14.02 ...
 *       current 400, 401 ...   power 5.60, 5.62 ...   energy 0.000, 0.001 ...
 *
 * If something is wrong
 *   Nothing lit at all     OE (PB2), or CLR on the shift register not tied high
 *   Digits in wrong order  DS1-DS4 swapped, edit digit_mask in display.c
 *   Garbled segments       segment bits do not match QA..QH, or segments are
 *                          active-low (see the comment in shift_out)
 *   All digits dim/ghosty  digit enables are active-high (see display_init)
 *   Flicker                raise DISPLAY_REFRESH_HZ in display.h
 *

#define F_CPU 2000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "display.h"

int main(void)
{
	display_init();
	sei();

	//---------------- Test 1: fixed numbers, one full scroll ----------------//
	display_update(12340, 5678, 90100, 1234);
	for (uint8_t seconds = 0; seconds < 12; seconds++) {
		_delay_ms(1000);
	}

	//---------------- Test 2: numbers that change every second ----------------//
	int32_t volts_mV   = 14000;
	int32_t current_mA = 400;
	int32_t power_mW   = 5600;
	int32_t energy_mWh = 0;

	while (1)
	{
		display_update(volts_mV, current_mA, power_mW, energy_mWh);

		volts_mV   = volts_mV + 10;
		current_mA = current_mA + 1;
		power_mW   = power_mW + 20;
		energy_mWh = energy_mWh + 1;

		_delay_ms(1000);
	}
}

*/