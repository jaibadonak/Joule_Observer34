/*
 * adc.c
 *
 * Created: 14/10/2026 5:07:59 pm
 *  Author: Admin
 */ 


#include "adc.h"
#include "common.h"

//void adc_init(void) {}
	
uint16_t adc_read_voltage(uint8_t chan){
	
	ADMUX &= ~(1 << MUX0);
	ADCSRA |= (1 << ADSC) ;
		
	while (ADCSRA & (1 << ADSC)){}
		
	return ADC;
}
uint16_t adc_read_current(uint8_t chan);{
	
	ADMUX |= (1 << MUX0);
	ADCSRA |= (1 << ADSC);
		
	while (ADCSRA & (1 << ADSC)){}
		
	return ADC;
}

uint16_t adc_convert_mA(uint16_t value) {
	
	return (uint16_t)(((uint32_t)value * 5000UL) / 1024UL);
	
}