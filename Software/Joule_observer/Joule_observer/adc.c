/*
 * adc.c
 *
 * Created: 14/10/2026 5:07:59 pm
 *  Author: Admin
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "adc.h"
#include "common.h"            // needs: #define samples 20 (or your value)

#define V_CH 0
#define I_CH 1


volatile uint16_t v_samples[samples];
volatile uint16_t i_samples[samples];
volatile uint16_t *vOrI_samples;

volatile uint8_t  channel = 0;          // channel being captured
static volatile uint8_t  armed = 0;               // 1 = the next INT0 edge starts a block
volatile uint8_t  flush = 0;               // next ADC interrupt is the leftover conversion
static volatile uint8_t  conversion_complete = 0;
volatile uint16_t sample_position;

// Called from ISR(INT0_vect) on every voltage zero crossing
void adc_trigger(void) {
	if (!armed) return;                           // not waiting for an edge, ignore it
	armed = 0;                                    // one trigger per block

	ADCSRB &= ~0x07;                              // free-running trigger source
	ADCSRA |= (1<<ADATE) | (1<<ADSC);             // start conversion
}

ISR(ADC_vect) {
	if (flush) {                                  // leftover conversion, discard
		flush = 0;
		channel = !channel;
		ADMUX = (ADMUX & 0xF0) | (channel ? I_CH : V_CH);
		vOrI_samples = channel ? i_samples : v_samples;

		if (channel == V_CH) {
			conversion_complete = 1;              // V and I both captured
			} else {
			armed = 1;                            // next INT0 edge starts the I block
		}
		} else {
		vOrI_samples[sample_position++] = ADC;
		if (sample_position >= samples) {
			ADCSRA &= ~(1<<ADATE);                // stop free-run
			flush = 1;                            // next interrupt is the leftover
		}
	}
}

void adc_init(void) {
	ADMUX  = (1<<REFS0);                                    // AVCC reference, channel 0
	ADCSRA = (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);         // /128
	ADCSRA |= (1<<ADEN);
	ADCSRA |= (1<<ADSC);                                    // dummy conversion
	while (ADCSRA & (1<<ADSC)) { }
	ADCSRA |= (1<<ADIF);                                    // clear flag (write 1)
	ADCSRA |= (1<<ADIE);                                    // enable interrupt
}

void adc_start_capture(void) {
	conversion_complete = 0;
	armed = 1;                                    // next INT0 edge starts the V block
}

uint8_t adc_capture_done(void) {
	return conversion_complete;
}




//NOTES:
//N1: Turns on auto-trigger (so conversions chain back-to-back in free-running mode) and starts the first conversion. 
//This is the moment sampling begins, locked to the signal edge. The tiem per sample is also consistent, as each sample is 13 clk cycles.
//N2: DUmmy conversion is important, the first firconversion after aden is enabled is takes 25 clocks, 
//to keep the conversion short and consistent, wait for this inital one to end.