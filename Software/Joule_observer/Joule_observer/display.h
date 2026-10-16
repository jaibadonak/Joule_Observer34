/*
 * display.h
 *
 * Created: 16/10/2026 4:54:54 pm
 *  Author: Admin
 */ 


#include <stdint.h>
#include <avr/io.h>

//---------------------------- Pins (from the schematic) ----------------------------//
// Shift register, bit-banged on port C
#define SH_CP  PC3      // shift clock, rising edge shifts one bit in
#define SH_DS  PC4      // serial data in
#define SH_ST  PC5      // storage (latch) clock, rising edge updates outputs

// Shift register output enable (OE, active low) on port B
#define OES    PB2

// Digit enables on port D. DS4 is the right-most digit, DS1 the left-most.
#define DS1    PD4
#define DS2    PD5
#define DS3    PD6
#define DS4    PD7
#define DIGITS_MASK  ((1<<DS1) | (1<<DS2) | (1<<DS3) | (1<<DS4))

//---------------------------- Display behaviour ----------------------------//
#define DISPLAY_DIGITS       4
#define DISPLAY_REFRESH_HZ   50      // whole display (all 4 digits) refreshed this often
#define DISPLAY_SCROLL_MS    3000    // time each measurement stays on screen

// Polarity. These match the lab board, check them against the display PCB.
#define DIGIT_ACTIVE_LOW     1       // 1: digit lit when its DSx pin is low
#define SEG_ACTIVE_HIGH      1       // 1: segment lit when its shift register bit is 1

// Segment bits as shifted out: QA = a ... QG = g, QH = dp (bit 0 ... bit 7)
#define SEG_DP     0x80
#define SEG_DASH   0x40              // segment g only
#define SEG_BLANK  0x00

// The measurements shown, in scroll order
typedef enum {
	DISPLAY_VOLTAGE = 0,
	DISPLAY_CURRENT,
	DISPLAY_POWER,
	DISPLAY_ENERGY,
	DISPLAY_QUANTITIES
} display_quantity_t;

// Set up the pins, clear the shift register and start the Timer0 scan interrupt.
// Does not enable global interrupts, call sei() afterwards.
void display_init(void);

// Give the display the latest measurements, in the same milli-units as the UART:
// mV (Vrms), mA (peak current), mW (power) and mWh (energy).
// Shown as:  14.00 V    841 mA    7.50 W    0.375 Wh (energy auto-ranges
// to 10.00 and 100.0 Wh). Out of range shows "----". Negative values show as 0.
void display_update(int32_t vrms_mV, int32_t ipk_mA, int32_t power_mW, int32_t energy_mWh);

// Lower level: load one measurement as number / 10^decimals (decimals 0 to 3).
// number above 9999 shows "----". Leading zeros are blanked.
void display_set_number(display_quantity_t quantity, uint32_t number, uint8_t decimals);

// The measurement currently on screen, e.g. to light a matching indicator LED.
display_quantity_t display_current_quantity(void);




#endif /* DISPLAY_H_ */