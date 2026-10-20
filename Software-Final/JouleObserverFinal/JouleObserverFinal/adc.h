//ADC Header

#ifndef ADC_H_
#define ADC_H_

#include "config.h"
#include <stdint.h>

#define samples 200             // samples in each block (one block of V, one of I). 200 samples is ~10 cycles

void    adc_init(void);
void    adc_start_capture(void);        // main: ask for a new V + I capture
void    adc_trigger(void);              // call from ISR(INT0_vect): "voltage zero crossing just happened"
uint8_t adc_capture_done(void);         // main: 1 once V and I are both captured

typedef struct {		//	Typedef Strut is a more clear and readable way to store variables. 
	uint16_t position;	//	All of the variable in the adc_status_t are different formats but all relate to the status of ADC.	
	uint16_t zero_crossings;
	uint8_t channel, armed, capturing, complete, mux, control;
} adc_status_t;
void adc_get_status(adc_status_t *status);

extern volatile uint16_t v_samples[samples];
extern volatile uint16_t i_samples[samples];

#endif /* ADC_H_ */