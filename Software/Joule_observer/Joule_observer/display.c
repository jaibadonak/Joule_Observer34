/*
 * display.c
 *
 * Created: 16/10/2026 4:55:06 pm
 *  Author: Admin
 */

#include "display.h"

#include <avr/interrupt.h>
#include <util/atomic.h>

//---------------------------- Display ----------------------------//
// Segment patterns for 0-9: bit0 = a ... bit6 = g (bit7 = dp is added separately)
static const uint8_t seg_pattern[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
0x6D, 0x7D, 0x07, 0x7F, 0x6F};
// U (voltage), A (amps), P (watts), E (Wh). Seven segments cannot draw V/W cleanly.
static const uint8_t quantity_pattern[DISPLAY_QUANTITIES] = {0x3E, 0x77, 0x73, 0x79};

// Digit enable pin masks in scan order. Index 0 is the least significant (right-most) digit.
// Stored as masks, not pin numbers, so the interrupt never needs a variable 1<<n.
// Bench confirmed DS1 is right-most and DS4 is left-most.
static const uint8_t digit_mask[DISPLAY_DIGITS] = {(1<<DS1), (1<<DS2), (1<<DS3), (1<<DS4)};

// Ready-to-shift patterns for every measurement, index 0 = least significant digit.
// The interrupt only ever shifts these out, all the maths happens in display_set_number.
static volatile uint8_t disp_characters[DISPLAY_QUANTITIES][DISPLAY_DIGITS];

// Snapshot of the measurement being drawn, taken at the start of each frame. A frame
// takes 4 ticks (20 ms), so without this an update landing mid-frame would show a
// mix of old and new digits. Only the interrupt touches it.
static uint8_t disp_frame[DISPLAY_DIGITS];

static volatile uint8_t  disp_position = 0;                 // digit drawn on the next tick
static volatile uint8_t  disp_quantity = DISPLAY_VOLTAGE;   // measurement on screen
static volatile uint16_t disp_frames   = 0;                 // frames since the last scroll



//---------------------------- Shift register ----------------------------//
// Shift one pattern into the register, MSB first so bit 7 (dp) ends up on QH.
// A segment lights when its bit is 1. If the display turns out to be active-low,
// add "pattern = ~pattern;" here.
static void shift_out(uint8_t pattern) {

	for (uint8_t bit = 0; bit < 8; bit++) {

		if (pattern & 0x80) PORTC |=  (1<<SH_DS);
		else                PORTC &= ~(1<<SH_DS);

		PORTC |=  (1<<SH_CP);                      // rising edge, bit shifted in
		PORTC &= ~(1<<SH_CP);                      // back low, ready for next edge

		pattern = pattern << 1;                    // next bit up to bit 7
	}
}

// Copy the shifted bits to the outputs.
static void latch(void) {
	PORTC |=  (1<<SH_ST);
	PORTC &= ~(1<<SH_ST);
}


//---------------------------- Public functions ----------------------------//
// A digit is lit when its enable pin is LOW (as in the lab board), so "all digits
// off" is DS1-DS4 high. If the display is active-high instead, swap the |= and &=
// on PORTD here and in the scan interrupt.
void display_init(void) {
	// Set each level before its direction so nothing glitches on while the
	// shift register still holds power-up garbage.

	// Keep the outputs disabled until the register holds something defined
	PORTB |=  (1<<OES);
	DDRB  |=  (1<<OES);

	// Shift register lines as outputs, clocks idle low
	PORTC &= ~((1<<SH_CP) | (1<<SH_ST));
	DDRC  |=  (1<<SH_CP) | (1<<SH_DS) | (1<<SH_ST);

	// Digit enables as outputs, all digits off to start
	PORTD |= DIGITS_MASK;
	DDRD  |= DIGITS_MASK;

	// Clear the register, then let it drive the segments
	shift_out(SEG_BLANK);
	latch();
	PORTB &= ~(1<<OES);

	// Nothing measured yet, show dashes
	for (uint8_t q = 0; q < DISPLAY_QUANTITIES; q++) {
		for (uint8_t i = 0; i < DISPLAY_DIGITS; i++) {
			disp_characters[q][i] = SEG_DASH;
		}
		disp_characters[q][0] = quantity_pattern[q];
	}
	disp_position = 0;
	disp_quantity = DISPLAY_VOLTAGE;
	disp_frames   = 0;

	// Timer0: CTC mode, compare match A interrupt at the tick rate, clk/64
	TCCR0A = (1<<WGM01);
	OCR0A  = (uint8_t)TIMER0_OCR;
	TIMSK0 = (1<<OCIE0A);
	TCCR0B = (1<<CS01) | (1<<CS00);
}

void display_set_number(uint8_t quantity, uint32_t number, uint8_t decimals) {
	uint8_t pattern[DISPLAY_DIGITS];

	if (quantity >= DISPLAY_QUANTITIES) return;
	pattern[0] = quantity_pattern[quantity]; // DS1/right-most digit is always the suffix

	if (number > 999 || decimals > 2) {
		for (uint8_t i = 1; i < DISPLAY_DIGITS; i++) pattern[i] = SEG_DASH;
	}
	else {
		uint16_t n = (uint16_t)number;

		for (uint8_t i = 0; i < DISPLAY_DIGITS - 1; i++) {
			// Blank leading zeros, but always show the digit just left of the dp (0.75, not .75)
			if (n == 0 && i > decimals) pattern[i + 1] = SEG_BLANK;
			else                        pattern[i + 1] = seg_pattern[n % 10];
			n = n / 10;
		}

		if (decimals > 0) {
			pattern[decimals + 1] |= SEG_DP;
		}
	}

	// Interrupts off while copying so the scan interrupt can't take its snapshot
	// halfway through. Restore the caller's interrupt state afterwards.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        for (uint8_t i = 0; i < DISPLAY_DIGITS; i++) {
            disp_characters[quantity][i] = pattern[i];
        }
    }
}

// Negative readings (noise around zero) are shown as 0
static uint32_t non_negative(int32_t value) {
	if (value < 0) return 0;
	return (uint32_t)value;
}

// Integer division that rounds to the nearest whole number instead of down
static uint32_t divide_round(uint32_t n, uint32_t d) {
	return (n + d / 2) / d;
}

// All inputs use milli-units; display V, A, W and Wh with up to two decimals.
// Drop a decimal when the rounded number would exceed the three numeric digits.
static void display_milli(uint8_t quantity, int32_t milli) {
    uint32_t value = non_negative(milli);
    uint32_t divisor = 10;
    uint8_t decimals = 2;
    while (divide_round(value, divisor) > 999 && decimals > 0) {
        divisor *= 10;
        decimals--;
    }
    display_set_number(quantity, divide_round(value, divisor), decimals);
}

void display_update(int32_t vrms_mV, int32_t irms_mA, int32_t power_mW, int32_t energy_mWh) {
    display_milli(DISPLAY_VOLTAGE, vrms_mV);
    display_milli(DISPLAY_CURRENT, irms_mA);
    display_milli(DISPLAY_POWER, power_mW);
    display_milli(DISPLAY_ENERGY, energy_mWh);
}

uint8_t display_current_quantity(void) {
	return disp_quantity;
}


//---------------------------- Scan interrupt ----------------------------//
// Fires every 5 ms and draws one digit.
ISR(TIMER0_COMPA_vect) {
	// Bit-banging a frame tick can exceed one 104 us ADC interval. Let
	// ADC/INT0 preempt it, but mask this source to prevent scan reentry.
	TIMSK0 &= ~(1<<OCIE0A);
	sei();
	uint8_t position = disp_position;

	// New frame: snapshot the measurement on screen so the whole frame is consistent
	if (position == 0) {
		if (disp_frames >= DISPLAY_SCROLL_FRAMES) {
			disp_frames = 0;
			disp_quantity = (disp_quantity + 1) % DISPLAY_QUANTITIES;
		}
		for (uint8_t i = 0; i < DISPLAY_DIGITS; i++) {
			disp_frame[i] = disp_characters[disp_quantity][i];
		}
	}

	// Shift the next digit's segments in while the previous digit is still lit
	shift_out(disp_frame[position]);

	// All digits off so the segments can change without ghosting, then latch and light the new digit
	PORTD |= DIGITS_MASK;
	latch();
	PORTD &= ~digit_mask[position];

	position++;
	if (position >= DISPLAY_DIGITS) {
		position = 0;

		// A full frame has been drawn. Only change measurement here so no frame mixes two.
		disp_frames++;
	}
	disp_position = position;
	cli();
	TIMSK0 |= (1<<OCIE0A);
}
