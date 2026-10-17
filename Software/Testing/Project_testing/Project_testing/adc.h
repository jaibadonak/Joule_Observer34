/*
 * adc.h
 *
 * Created: 17/10/2026 12:30:42 pm
 *  Author: Admin
 */ 


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