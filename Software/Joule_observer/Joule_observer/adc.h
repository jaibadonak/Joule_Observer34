/*
 * adc.h
 *
 * Created: 14/10/2026 5:05:33 pm
 *  Author: Admin
 */ 


#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>

//void     adc_init(void);

uint16_t adc_read_voltage(uint8_t chan);
uint16_t adc_read_current(uint8_t chan);

uint16_t adc_convert_mA(uint16_t value);
void    adc_start_capture(void);



#endif /* ADC_H_ */