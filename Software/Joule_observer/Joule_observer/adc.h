/*
 * adc.h
 *
 * Created: 14/10/2026 5:05:33 pm
 *  Author: Admin
 */ 


#ifndef ADC_H_
#define ADC_H_
#include <stdint.h>

#define samples 200

void    adc_init(void);
void    adc_start_capture(void);        // main: ask for a new V + I capture
void    adc_trigger(void);              // call from ISR(INT0_vect): "voltage zero crossing just happened"
uint8_t adc_capture_done(void);         // main: 1 once V and I are both captured

extern volatile uint16_t v_samples[];
extern volatile uint16_t i_samples[];

#endif