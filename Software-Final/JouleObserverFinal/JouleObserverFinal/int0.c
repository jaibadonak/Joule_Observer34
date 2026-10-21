// Voltage zero crossing on PD2 starts each capture on the rising edge.
#include <avr/io.h>
#include <avr/interrupt.h>
#include "adc.h"
#include "int0.h"

// The AVR jumps here automatically when the enabled INT0 input detects a rising edge.
// Keep this short: adc_trigger() starts a block only if the ADC is waiting for one.
ISR(INT0_vect) {
    adc_trigger();
}

// Configure PD2 as an input and tell interrupt INT0 which change to look for.
void int0_init(void) {
    // DDRD controls pin direction. Clearing bit DDD2 makes PD2 an input.
    // 1<<DDD2 selects that bit, ~ reverses the mask, and &= clears only that bit.
    DDRD &= ~(1<<DDD2);
    // With PD2 configured as an input, clearing its PORTD bit turns off the internal pull-up.
    PORTD &= ~(1<<PORTD2);
    // EICRA controls external interrupt sensing. ISC01=1 and ISC00=1 select a rising edge
    // (LOW to HIGH). Clear both bits first, then set both, keeping the other settings.
    EICRA = (EICRA & ~((1<<ISC01) | (1<<ISC00))) | (1<<ISC01) | (1<<ISC00);
    // EIFR holds pending interrupt flags. Writing 1 to INTF0 clears an old event.
    // This register clears flags by writing 1, unlike an ordinary stored setting.
    EIFR = (1<<INTF0);
    // EIMSK enables individual external interrupts. |= sets INT0 without clearing other bits.
    // Global interrupts still need sei() in main before the ISR can run.
    EIMSK |= (1<<INT0);
}
