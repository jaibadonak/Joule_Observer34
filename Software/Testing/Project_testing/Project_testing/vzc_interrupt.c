/*
 * vzc_interrupt.c
 *
 * Created: 17/10/2026 12:34:26 pm
 *  Author: Admin
 */ 

#include <avr/io.h>
#include "adc.h"

void int0_init(){
	EICRA |= (1<<ISC01) | (1<<ISC00);   // rising edge
	EIMSK |= (1<<INT0);
}

ISR(INT0){
	adc_trigger();
}