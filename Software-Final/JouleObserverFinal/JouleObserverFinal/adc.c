// ADC 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/atomic.h>
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
static volatile uint8_t  capturing = 0;               // ignore stale interrupts while idle
static volatile uint16_t sample_position = 0;         // next free slot in the current block
static volatile uint16_t zero_crossings = 0;

// This is called when our zero crossing detector goes off.
void adc_trigger(void) {
	zero_crossings++;                             // edge counter
	if (!armed) return;                           // not waiting for an edge, ignore it
	armed = 0;                                    // one trigger per block
	sample_position = 0;                          // start filling the block from the beginning
	capturing = 1;

	ADCSRB &= ~0x07;                              // free-running trigger source
	ADCSRA |= (1<<ADATE) | (1<<ADSC);             // start conversion, hardware chains the rest
}

// This is for writing values to its relevant array.
ISR(ADC_vect) {
	uint16_t value = ADC;                         // read result before pointer/index work
	if (!capturing) return;						  // If we are still capturing, return
	if (flush) {                                  // leftover conversion, discard it
		flush = 0;
		capturing = 0;
		channel = !channel;
		ADMUX = (ADMUX & 0xF0) | (channel ? I_CH : V_CH);
		vOrI_samples = channel ? i_samples : v_samples;

		if (channel == V_CH) {
			conversion_complete = 1;              // V and I both captured
			} else {
			armed = 1;                            // next INT0 edge starts the I block
		}
		} else {
		// write to our relevant array as long as our sample position is less than our samples.
		if (sample_position < samples) { vOrI_samples[sample_position++] = value; }
		if (sample_position >= samples) {
			ADCSRA = (1<<ADEN) | (1<<ADIE) | (1<<ADPS2);
			flush = 1;                            // next interrupt is the leftover
		}
	}
}

void adc_init(void) {
	// Set input direction and disable pull-ups explicitly, including after a bootloader.
	DDRC &= ~((1<<PINC0) | (1<<PINC1));						//inputs
	PORTC &= ~((1<<PINC0) | (1<<PINC1));					//disable any pull ups(if any).
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
	// Also safe on timeout: abort any old conversion before resetting its state.
	//N4
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		ADCSRA = 0;
		channel = V_CH;
		ADMUX = (1<<REFS0) | V_CH;
		vOrI_samples = v_samples;
		sample_position = 0;
		flush = 0;
		conversion_complete = 0;
		capturing = 0;
		ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADIF);
		// Warm up after ADEN (25 clocks), before waiting for the edge.
		ADCSRA |= (1<<ADSC);
		while (ADCSRA & (1<<ADSC)) { }
		ADCSRA |= (1<<ADIF) | (1<<ADIE);
		EIFR = (1<<INTF0);  // ignore an edge that arrived during warm-up
		armed = 1;
	}
}
uint8_t adc_capture_done(void) {
	return conversion_complete;
}
//copies all private ADC state into the struct
//needed because those variables are static, so main can't read them directly
//similar idea to private fields and gett functions in java.
void adc_get_status(adc_status_t *status) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		status->position = sample_position;
		status->zero_crossings = zero_crossings;
		status->channel = channel;
		status->armed = armed;
		status->capturing = capturing;
		status->complete = conversion_complete;
		status->mux = ADMUX;
		status->control = ADCSRA;
	}
}


//NOTES:
//N1: adc_trigger turns on auto-trigger (conversions chain back-to-back in free-running mode) and starts the
//first conversion. Sampling begins locked to the voltage zero crossing. Each sample takes 13 ADC clocks.
//N2: In free-running mode the next conversion has already started when the ADC interrupt fires, so after the
//last sample there is one leftover conversion. It is discarded, and it is the one safe moment to change channel.
//N3: The dummy conversion in adc_init is needed because the first conversion after ADEN takes 25 clocks.
//
//N4: TL;DR it turns off interrupts breifly to run the block of code, to avoid any errors and then turns them back on (if they were on).
//An interrupt ca nfire between any two machine instructions in main. Usually harmless, but not when main and an ISR share variables.
//example: Example of a torn read. AVR is 8-bit, so reading a 16-bit sample_position takes two instructions (low byte, then high byte).
//
//sample_position = 0x00FF (255)
//
//main reads low byte   → 0xFF
//   ADC ISR fires, sample_position++ -> 0x0100 (256)
//main reads high byte  → 0x01
//
//main thinks it's 0x01FF = 511    (was never true)
//ATOMIC_BLOCK(ATOMIC_RESTORESTATE) fixes this:
//1) Saves SRG
//2) cli(), interrupts off
//3) runs the codd,nothing can interrupt it
//restores SREG, so interrupts go back to normal.


