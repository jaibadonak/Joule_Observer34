/*
 * adc.c
 *
 * Created: 17/10/2026 12:30:26 pm
 *  Author: Admin
 *

#include "adc.h"

static volatile uint8_t chan = 0;          // channel being captured
static volatile uint8_t  armed = 0;        // 1 = the next INT0 edge starts a block
volatile uint8_t  flush = 0;               // next ADC interrupt is the leftover conversion
static volatile uint8_t  conversion_complete = 0;
volatile uint16_t sample_position;


void adc_init(){
	ADMUX |= (1 << REFS0);
	ADCSRA = (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);         // 128
	ADCSRA |= (1<<ADEN) | (1<<ADSC);
	while (ADCSRA & (1<<ADSC)) { }
	ADCSRA |= (1<<ADIF) | (1<<ADIE);                       // clear flag (write 1) & enable interrupt
	
}

void adc_trigger(void) {
	if (!armed) return;                           // not waiting for an edge, ignore it
	armed = 0;                                    // one trigger per block

	ADCSRB &= ~0x07;                              // free-running trigger source
	ADCSRA |= (1<<ADATE) | (1<<ADSC);             // start conversion
}

//_____________________ Super sampling method _____________________//

/*
Positive edge trigger
read 200 samples over 10 periods
sort samples (count from ~2.1 to max then to min then to 2.1)
store
switch channel
repeat


void adc_start_capture(){
	ADCSRA &= ~(1<<ADATE);                        // make sure free-run is off
	chan = V_CH;								  // Start with voltage
	ADMUX = (ADMUX & 0xF0) | V_CH;				  // Switch to voltage channel
	vOrI_samples = v_samples;					  // Point to Voltage samples
	
	sample_position = 0;
	flush = 0;
	conversion_complete = 0;
	armed = 1;                                    // next INT0 edge starts the V block
}

void
*/

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

#define V_CH 0                  // ADC0 (PC0): voltage
#define I_CH 1                  // ADC1 (PC1): current

volatile uint16_t v_samples[samples];
volatile uint16_t i_samples[samples];

static volatile uint16_t *vOrI_samples = v_samples;   // where the next result is stored
static volatile uint8_t  channel = V_CH;              // channel being captured
static volatile uint8_t  armed = 0;                   // 1 = the next INT0 edge starts a block
static volatile uint8_t  flush = 0;                   // 1 = the next ADC interrupt is the leftover conversion
static volatile uint8_t  conversion_complete = 0;     // 1 = V and I blocks are both full
static volatile uint16_t sample_position = 0;         // next free slot in the current block


// Called from ISR(INT0_vect) on every voltage zero crossing
void adc_trigger(void) {
	if (!armed) return;                           // not waiting for an edge, ignore it
	armed = 0;                                    // one trigger per block
	sample_position = 0;                          // start filling the block from the beginning

	ADCSRB &= ~0x07;                              // free-running trigger source
	ADCSRA |= (1<<ADATE) | (1<<ADSC);             // start conversion, hardware chains the rest
}

// Fires when a conversion finishes. The hardware has already started the next one.
ISR(ADC_vect) {
	if (flush) {                                  // leftover conversion, discard it
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
	DIDR0  = (1<<ADC0D) | (1<<ADC1D);                       // PC0, PC1 analogue only
	ADMUX  = (1<<REFS0);                                    // AVCC reference, right adjusted, channel 0
	ADCSRA = (1<<ADPS2);                                    // /16: 125 kHz ADC clock, 104 us per sample (9.6 kHz)
	ADCSRA |= (1<<ADEN);                                    // enable the ADC
	ADCSRA |= (1<<ADSC);                                    // dummy conversion (the first one takes 25 clocks)
	while (ADCSRA & (1<<ADSC)) { }                          // wait for it to finish
	ADCSRA |= (1<<ADIF);                                    // clear the flag (write 1)
	ADCSRA |= (1<<ADIE);                                    // enable the ADC interrupt
}

// Reset the capture state and wait for the next voltage zero crossing.
// Call this from main when the ADC is idle (at start up, or after a capture is done).
void adc_start_capture(void) {
	ADCSRA &= ~(1<<ADATE);                        // make sure free-run is off
	channel = V_CH;
	ADMUX = (ADMUX & 0xF0) | V_CH;
	vOrI_samples = v_samples;
	sample_position = 0;
	flush = 0;
	conversion_complete = 0;
	armed = 1;                                    // next INT0 edge starts the V block
}

uint8_t adc_capture_done(void) {
	return conversion_complete;
}


//NOTES:
//N1: adc_trigger turns on auto-trigger (conversions chain back-to-back in free-running mode) and starts the
//first conversion. Sampling begins locked to the voltage zero crossing. Each sample takes 13 ADC clocks.
//N2: In free-running mode the next conversion has already started when the ADC interrupt fires, so after the
//last sample there is one leftover conversion. It is discarded, and it is the one safe moment to change channel.
//N3: The dummy conversion in adc_init is needed because the first conversion after ADEN takes 25 clocks.