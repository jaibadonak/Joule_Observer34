/*
 * adc.c
 *
 * Created: 14/10/2026 5:07:59 pm
 *  Author: Admin
 */ 

#include <stdint.h>
#include "interrupts.h"
#include "adc.h"
#include "common.h"
#define samples 400
volatile uint8_t  = 0;
volatile uint16_t sample_position = 0;
volatile uint8_t flush //a variable to ensure we are starting fresh conversions
volatile uint8_t conversion_ready = 0;
volatile uint16_t v_samples[samples];
volatile uint16_t i_samples[samples];
volatile uint16_t *vOrI_samples = v_samples;

ISR (INT0_vect) {
	EIMSK &= ~(1<<INT0) //turn it off
	sample_position = 0; 
	ADCSRB &= ~(0x07);//free-running-mode, if not already.
	ADCSRA |= (1<<ADATE)|(1<<ADSC);// start conversion (N1)
}

ISR (ADC_vect) {
	if (flush) {
		flush = 0;
		channel = !channel; //toggle
		ADMUX = (ADMUX & 0xF0) | channel;
		if (channel != 0) vOrI_samples = i_samples;   // channel is 1, recording current
		else              vOrI_samples = v_samples;   // channel is 0, recording voltage
		if (!(channel)) conversion_ready = 1; 
		else {
			EIFR  = (1<<INTF0);              // clear stale edge flag
            EIMSK |= (1<<INT0);              // re
		}
	} else {
		sample_position++;
		vOrI_samples[sample_position] = ADC //record value and point to current sampling array.
		if (sample_position >= samples) {
			 ADCSRA &= ~(1<<ADATE);       // stop free-run
			 flush = 1;                   // next interrupt is the leftover
		}

	}
}

//void adc_init(void) {}
	
uint16_t adc_read_voltage(uint8_t chan){
	
	ADMUX &= ~(1 << MUX1); //turn off current channel.
	ADMUX |= (1<<MUX0);//turn on voltage channel.

	ADCSRA |= (1 << ADSC) ;
		
	while (ADCSRA & (1 << ADSC)){}
		
	return ADC;
}
uint16_t adc_read_current(uint8_t chan);{
	
	ADMUX &= ~(1 << MUX0); //turn off voltage channel.
	ADMUX |= (1<<MUX1);//turn on current channel.

	ADCSRA |= (1 << ADSC);
		
	while (ADCSRA & (1 << ADSC)){}
		
	return ADC;
}

uint16_t adc_to_voltage(uint16_t value) {
	
	return (uint16_t)(((uint32_t)value * 5000UL) / 1024UL);
	
}





//NOTES:
//N1: Turns on auto-trigger (so conversions chain back-to-back in free-running mode) and starts the first conversion. 
//This is the moment sampling begins, locked to the signal edge.