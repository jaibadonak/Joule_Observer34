/*
 * adc.c
 *
 * Created: 17/10/2026 12:30:26 pm
 *  Author: Admin
 */ 

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
*/

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
