/*
 * display.c
 *
 * Created: 16/10/2026 4:55:06 pm
 *  Author: Admin
 */

#include "display.h"
#include <avr/interrupt.h>
#include <util/atomic.h>

// Define the CPU clock speed if it has not already been defined elsewhere.
#ifndef F_CPU
#define F_CPU 2000000UL
#endif

//---------------------------- Timing ----------------------------//
// One digit is drawn per timer tick, so the tick rate is digits x refresh rate
// (4 x 50 Hz = 200 Hz, a digit every 5 ms).
#define DISPLAY_TICK_HZ       (DISPLAY_REFRESH_HZ * DISPLAY_DIGITS)

// Timer0 in CTC mode, prescaler 64: 2 MHz / 64 = 31.25 kHz, OCR0A = 155 gives
// 31.25 kHz / 156 = 200.3 Hz (0.16% fast, so about 50.08 Hz refresh).
#define TIMER0_PRESCALE       64UL
#define TIMER0_OCR            (((F_CPU + (TIMER0_PRESCALE * DISPLAY_TICK_HZ) / 2) \
                                / (TIMER0_PRESCALE * DISPLAY_TICK_HZ)) - 1UL)
#if TIMER0_OCR > 255
#error "Timer0 cannot reach the display tick rate, use a bigger prescaler"
#endif

// Frames (complete scans of all digits) per measurement: 3 s x 50 Hz = 150
#define DISPLAY_SCROLL_FRAMES ((uint16_t)((DISPLAY_SCROLL_MS * (uint32_t)DISPLAY_REFRESH_HZ) / 1000UL))

//---------------------------- Digit enable polarity ----------------------------//
#if DIGIT_ACTIVE_LOW
#define DIGITS_OFF()    (PORTD |=  DIGITS_MASK)
#define DIGIT_ON(mask)  (PORTD &= ~(mask))
#else
#define DIGITS_OFF()    (PORTD &= ~DIGITS_MASK)
#define DIGIT_ON(mask)  (PORTD |=  (mask))
#endif

//---------------------------- Data ----------------------------//
// Segment patterns for 0-9: bit0 = a ... bit6 = g (bit7 = dp is added separately)
static const uint8_t seg_pattern[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                        0x6D, 0x7D, 0x07, 0x7F, 0x6F};

// Digit enable pin masks in scan order. Index 0 is the least significant (right-most) digit.
// Stored as masks, not pin numbers, so the interrupt never needs a variable 1<<n.
static const uint8_t digit_mask[DISPLAY_DIGITS] = {(1<<DS4), (1<<DS3), (1<<DS2), (1<<DS1)};

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
// Inlined and shifting the pattern left (instead of 1<<i) because the AVR has
// no barrel shifter, and this runs inside the scan interrupt.
static inline __attribute__((always_inline)) void shift_out(uint8_t pattern) {
#if !SEG_ACTIVE_HIGH
	pattern = ~pattern;
#endif

	for (uint8_t bit = 0; bit < 8; bit++) {

		if (pattern & 0x80) PORTC |=  (1<<SH_DS);
		else                PORTC &= ~(1<<SH_DS);

		PORTC |=  (1<<SH_CP);                      // rising edge, bit shifted in
		PORTC &= ~(1<<SH_CP);                      // back low, ready for next edge

		pattern <<= 1;
	}
}

// Copy the shifted bits to the outputs.
static inline __attribute__((always_inline)) void latch(void) {
	PORTC |=  (1<<SH_ST);
	PORTC &= ~(1<<SH_ST);
}


//---------------------------- Public functions ----------------------------//
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
	DIGITS_OFF();
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

void display_set_number(display_quantity_t quantity, uint32_t number, uint8_t decimals) {
	uint8_t pattern[DISPLAY_DIGITS];

	if (quantity >= DISPLAY_QUANTITIES) return;

	if (number > 9999) {
		for (uint8_t i = 0; i < DISPLAY_DIGITS; i++) pattern[i] = SEG_DASH;
	}
	else {
		uint16_t n = (uint16_t)number;

		for (uint8_t i = 0; i < DISPLAY_DIGITS; i++) {
			// Blank leading zeros, but always show the digit just left of the dp (0.75, not .75)
			if (n == 0 && i > decimals) pattern[i] = SEG_BLANK;
			else                        pattern[i] = seg_pattern[n % 10];
			n /= 10;
		}

		if (decimals > 0 && decimals < DISPLAY_DIGITS) {
			pattern[decimals] |= SEG_DP;
		}
	}

	// Copy under a lock so the interrupt's snapshot never catches it half written
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		for (uint8_t i = 0; i < DISPLAY_DIGITS; i++) {
			disp_characters[quantity][i] = pattern[i];
		}
	}
}

// Negative readings (noise around zero) are shown as 0
static uint32_t non_negative(int32_t value) {
	return (value > 0) ? (uint32_t)value : 0;
}

static uint32_t divide_round(uint32_t n, uint32_t d) {
	return (n + d / 2) / d;
}

void display_update(int32_t vrms_mV, int32_t ipk_mA, int32_t power_mW, int32_t energy_mWh) {
	uint32_t energy = non_negative(energy_mWh);
	uint8_t  energy_decimals = 3;

	display_set_number(DISPLAY_VOLTAGE, divide_round(non_negative(vrms_mV), 10), 2);   // 14.00 V
	display_set_number(DISPLAY_CURRENT, non_negative(ipk_mA), 0);                      // 841 mA
	display_set_number(DISPLAY_POWER,   divide_round(non_negative(power_mW), 10), 2);  // 7.50 W

	// Energy in Wh: 0.000 - 9.999, then lose a decimal each time it outgrows 4 digits
	while (energy > 9999 && energy_decimals > 0) {
		energy = divide_round(energy, 10);
		energy_decimals--;
	}
	display_set_number(DISPLAY_ENERGY, energy, energy_decimals);
}

display_quantity_t display_current_quantity(void) {
	return (display_quantity_t)disp_quantity;
}


//---------------------------- Scan interrupt ----------------------------//
// Fires every 5 ms and draws one digit. Measured in simulation at 2 MHz: about 110 us
// on average, 145 us worst case (the first digit of a frame), none of it waiting.
// Other interrupts are held off for that long.
ISR(TIMER0_COMPA_vect) {
	uint8_t position = disp_position;

	// New frame: snapshot the measurement on screen so the whole frame is consistent
	if (position == 0) {
		for (uint8_t i = 0; i < DISPLAY_DIGITS; i++) {
			disp_frame[i] = disp_characters[disp_quantity][i];
		}
	}

	// Shift the next digit's segments in while the previous digit is still lit
	shift_out(disp_frame[position]);

	// All digits off so the segments can change without ghosting, then latch and light the new digit
	DIGITS_OFF();
	latch();
	DIGIT_ON(digit_mask[position]);

	position++;
	if (position >= DISPLAY_DIGITS) {
		position = 0;

		// A full frame has been drawn. Only change measurement here so no frame mixes two.
		disp_frames++;
		if (disp_frames >= DISPLAY_SCROLL_FRAMES) {
			disp_frames = 0;
			disp_quantity = (disp_quantity + 1) % DISPLAY_QUANTITIES;
		}
	}
	disp_position = position;
}
