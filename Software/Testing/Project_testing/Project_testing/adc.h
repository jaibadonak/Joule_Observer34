/*
 * adc.h
 *
 * Created: 17/10/2026 12:30:42 pm
 *  Author: Admin
 


#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>
#include <avr/io.h>

#define sample_size 200;
#define V_CH 0;
#define I_CH 1;

extern volatile uint16_t v_samples[sample_size];
extern volatile uint16_t i_samples[sample_size];
extern volatile uint16_t *vOrI_samples;

void adc_init(void);

void    adc_start_capture(void);        // main: ask for a new V + I capture

void    adc_trigger(void);              // call from ISR(INT0_vect): "voltage zero crossing just happened"

uint8_t adc_capture_done(void);         // main: 1 once V and I are both captured

void adc_sort(void);


#endif /* ADC_H_ */

/*
 * adc.h
 *
 * Created: 14/10/2026 5:05:33 pm
 *  Author: Admin
 *
 * Captures one block of voltage samples and one block of current samples.
 * Both blocks start on a voltage zero crossing (INT0), so v_samples[k] and
 * i_samples[k] are taken at the same point in the cycle.
 *
 * Usage:
 *     adc_init();  int0_init();  sei();
 *     adc_start_capture();
 *     ... wait until adc_capture_done() is 1 ...
 *     use v_samples[] and i_samples[]
 *     adc_start_capture();                  // for the next capture
 */


#ifndef ADC_H_
#define ADC_H_
#include <stdint.h>

#define samples 200             // samples in each block (one block of V, one of I)

void    adc_init(void);
void    adc_start_capture(void);        // main: ask for a new V + I capture
void    adc_trigger(void);              // call from ISR(INT0_vect): "voltage zero crossing just happened"
uint8_t adc_capture_done(void);         // main: 1 once V and I are both captured

extern volatile uint16_t v_samples[samples];
extern volatile uint16_t i_samples[samples];

#endif /* ADC_H_ */

