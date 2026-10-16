/*
 * volt_interrupts.c
 *
 * Created: 15/10/2026 2:39:03 pm
 *  Author: Admin
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#define VZC_PIN PD2 //aka int0 pin
#define Signal_High INT0_vect
#include <stdint.h>
#include "interrupts.h"
#include "common.h"
#include "adc.h"

//volatile uint8_t firstEdge = 0;

ISR(INT0_vect) {
	adc_trigger();                  // first line, so the ADC starts with minimal delay
	// ...your Timer1 code, if you keep it...
}

void int0_init(void) {
	EICRA |= (1<<ISC01) | (1<<ISC00);   // rising edge
	EIMSK |= (1<<INT0);
}
/*ISR(INT0_vect) {

    TCNT1 = RecordValue;
    TCNT1 = 0; //reset inital value
	TCCR1B |= (1<<CS10); //start count
    
	if (firstEdge) {
        TCCR1B &= ~(1 << CS10);
        firstEdge = 0;
    }
    firstEdge = 1;
}

//LM393 is high when voltage is rising, low when dropping.

void int0_init() {
	EICRA |= (1<<ISC00) | (1<< ISC01); // 11 in register for rising edge
	EIMSK |= (1<<INT0); //enables specific interrupt pin
}
*/