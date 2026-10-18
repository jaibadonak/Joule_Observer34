#ifndef ADC_H_
#define ADC_H_
#include "config.h"
#include <stdint.h>

#define samples 200             // samples in each block (one block of V, one of I)

void    adc_init(void);
void    adc_start_capture(void);        // main: ask for a new V + I capture
void    adc_trigger(void);              // call from ISR(INT0_vect): "voltage zero crossing just happened"
uint8_t adc_capture_done(void);         // main: 1 once V and I are both captured

typedef struct {
    uint16_t position;
    uint16_t zero_crossings;
    uint8_t channel, armed, capturing, complete, mux, control;
} adc_status_t;
void adc_get_status(adc_status_t *status); // atomic diagnostic snapshot

extern volatile uint16_t v_samples[samples];
extern volatile uint16_t i_samples[samples];

#endif /* ADC_H_ */
