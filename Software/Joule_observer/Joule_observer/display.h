/*
 * display.h
 *
 * Created: 16/10/2026 4:54:54 pm
 *  Author: Admin
 *
 * 4-digit 7-segment display, multiplexed through one shift register (segments)
 * plus four digit enables on the MCU.
 *
 * A Timer0 interrupt draws one digit every 5 ms, so all four digits are
 * refreshed 50 times a second. The measurement shown changes every 3 s:
 * V -> I -> P -> E -> V ...
 *
 * Usage (assumes the 2 MHz system clock):
 *     display_init();
 *     sei();
 *     ...
 *     display_update(vrms_mV, irms_mA, power_mW, energy_mWh);
 *
 * Call display_update from main, not from an interrupt.
 */


#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>
#include <avr/io.h>
#include "config.h"

//---------------------------- Pins (from the schematic) ----------------------------//
// Shift register, bit-banged on port C
#define SH_CP  PC3      // shift clock, rising edge shifts one bit in
#define SH_DS  PC4      // serial data in
#define SH_ST  PC5      // storage (latch) clock, rising edge updates outputs

// Shift register output enable (OE, active low) on port B
#define OES    PB2

// Digit enables on port D. Bench confirmed DS1 is right-most, DS4 left-most.
#define DS1    PD4
#define DS2    PD5
#define DS3    PD6
#define DS4    PD7
#define DIGITS_MASK  ((1<<DS1) | (1<<DS2) | (1<<DS3) | (1<<DS4))

//---------------------------- Display behaviour ----------------------------//
#define DISPLAY_DIGITS         4
#define DISPLAY_REFRESH_HZ     50      // whole display (all 4 digits) refreshed this often
#define DISPLAY_SCROLL_SECONDS 3       // time each measurement stays on screen
#define DISPLAY_SCROLL_FRAMES  (DISPLAY_SCROLL_SECONDS * DISPLAY_REFRESH_HZ)

// Timer0 compare value. One tick draws one digit, so the tick rate is
// 4 x 50 Hz = 200 Hz. With a 2 MHz clock and a /64 prescaler:
// 2000000 / 64 / 200 - 1 = 155. Keep DISPLAY_REFRESH_HZ at 31 or more, or this
// no longer fits in the 8-bit OCR0A register.
#define TIMER0_OCR  ((F_CPU / 64UL / (DISPLAY_REFRESH_HZ * DISPLAY_DIGITS)) - 1UL)

// Segment bits as shifted out: QA = a ... QG = g, QH = dp (bit 0 ... bit 7)
#define SEG_DP     0x80
#define SEG_DASH   0x40              // segment g only
#define SEG_BLANK  0x00

// The measurements shown, in scroll order
#define DISPLAY_VOLTAGE     0
#define DISPLAY_CURRENT     1
#define DISPLAY_POWER       2
#define DISPLAY_ENERGY      3
#define DISPLAY_QUANTITIES  4

// Set up the pins, clear the shift register and start the Timer0 scan interrupt.
// Does not enable global interrupts, call sei() afterwards.
void display_init(void);

// Give the display the latest measurements, in the same milli-units as the UART:
// mV (Vrms), mA (RMS current), mW (power) and mWh (energy).
// Right-most digit is a suffix: U=voltage, A=current, P=power, E=energy.
// Three numeric digits show V, A, W and Wh: 13.7U, 0.40A, 5.48P, 0.12E.
// Up to two decimals; reduce precision as values grow (9.99, 10.0, 100).
// Out of range shows three dashes plus the suffix. Negative values show zero.
void display_update(int32_t vrms_mV, int32_t irms_mA, int32_t power_mW, int32_t energy_mWh);

// Lower level: load one measurement (DISPLAY_VOLTAGE ... DISPLAY_ENERGY) as
// number / 10^decimals (decimals 0 to 2), followed by the quantity letter.
// number above 999 shows three dashes plus the suffix. Leading zeros are blanked.
void display_set_number(uint8_t quantity, uint32_t number, uint8_t decimals);

// The measurement currently on screen, also reported to the website over UART.
uint8_t display_current_quantity(void);

#endif /* DISPLAY_H_ */
