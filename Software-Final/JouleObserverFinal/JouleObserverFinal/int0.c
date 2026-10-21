// Voltage zero crossing on PD2 starts each capture on the rising edge.
#include <avr/io.h>
#include <avr/interrupt.h>
#include "adc.h"
#include "int0.h"

ISR(INT0_vect) {
    adc_trigger();
}

void int0_init(void) {
    DDRD &= ~(1<<DDD2);
    PORTD &= ~(1<<PORTD2);
    EICRA = (EICRA & ~((1<<ISC01) | (1<<ISC00))) | (1<<ISC01) | (1<<ISC00);
    EIFR = (1<<INTF0);
    EIMSK |= (1<<INT0);
}
