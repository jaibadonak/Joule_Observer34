/*
 * vzc_interrupt.c
 *
 * Created: 17/10/2026 12:34:26 pm
 *  Author: Admin
 *
#include <avr/io.h>
#include "adc.h"

void int0_init(){
	EICRA |= (1<<ISC01) | (1<<ISC00);   // rising edge
	EIMSK |= (1<<INT0);
}

ISR(INT0){
	adc_trigger();
}
*/

/*
 * volt_interrupts.c
 *
 * Created: 15/10/2026 2:39:03 pm
 *  Author: Admin
 *
 * Voltage zero crossing on INT0 (PD2). The comparator output goes high when
 * the mains voltage crosses zero going up, so INT0 fires on the rising edge.
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include "interrupts.h"
#include "adc.h"

ISR(INT0_vect) {
	adc_trigger();                      // starts a capture if one is armed, otherwise ignores the edge
}

void int0_init(void) {
	EICRA |= (1<<ISC01) | (1<<ISC00);   // rising edge
	EIFR   = (1<<INTF0);                // clear any edge already seen
	EIMSK |= (1<<INT0);                 // enable INT0
}