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

#define V_CH 0
#define I_CH 1
volatile uint8_t 
volatile uint16_t sample_position = 0;
volatile uint8_t flush //a variable to ensure we are starting fresh conversions
static volatile uint8_t conversion_complete = 0;
volatile uint16_t v_samples[samples];
volatile uint16_t i_samples[samples];
volatile uint16_t *vOrI_samples = v_samples;

ISR(INT0_vect) {
    EIMSK &= ~(1<<INT0);                    // one trigger per capture
    sample_position = 0;
    ADCSRB &= ~0x07;                        // free-running trigger source
    ADCSRA |= (1<<ADATE) | (1<<ADSC);       // start conversion
}

ISR(ADC_vect) {
    if (flush) {                            // leftover conversion, discard
        flush = 0;
        channel = !channel;
        ADMUX = (ADMUX & 0xF0) | (channel ? I_CH : V_CH);
        vOrI_samples = channel ? i_samples : v_samples;

        if (channel == 0) {
            conversion_complete = 1;        // V and I both captured
        } else {
            EIFR  = (1<<INTF0);             // clear stale edge flag
            EIMSK |= (1<<INT0);             // arm for the I capture
        }
    } else {
        vOrI_samples[sample_position++] = ADC;
        if (sample_position >= samples) {
            ADCSRA &= ~(1<<ADATE);          // stop free-run
            flush = 1;                      // next interrupt is the leftover
        }
    }
}

void adc_init(void) {
	ADMUX |= (1<< REFS0);
	ADMUX &= ~(1<<REFS1);    //AVCC as source voltage.
	ADCSRA = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);   // 128 prescaler
	//---DUMMY Conversion---//(N2)
	ADCSRA |= (1 << ADEN);
	while (ADSCRA & (1<<ADSC));
	ADC = 0;
	ADCSRA |= (1 << ADIF); // clear the flag (write 1 to clear)
	ADCSRA |= (1<<ADIE)    //enable interrupt

}

void adc_start_capture(void) {
    conversion_complete = 0;
    EIFR  = (1 << INTF0);
    EIMSK |= (1 << INT0);
}
	
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
//This is the moment sampling begins, locked to the signal edge. The tiem per sample is also consistent, as each sample is 13 clk cycles.
//N2: DUmmy conversion is important, the first firconversion after aden is enabled is takes 25 clocks, 
//to keep the conversion short and consistent, wait for this inital one to end.