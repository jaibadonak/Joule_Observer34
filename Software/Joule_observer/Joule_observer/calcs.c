/*
 * calcs.c
 *
 * Created: 15/10/2026 12:25:14 pm
 *  Author: Admin
 */ 
#define OFFSET 2.0625
#include <math.h>
#include "adc.h"
#include "common.h"
#include "calcs.h"

volatile uint32_t instpower[samples];

void compute_instPower(uint16_t v_samples[samples], uint16_t i_samples[samples]) {
    for(uint8_t i = 0; i <= samples; i++) {
        uint32_t rawvalue =  v_samples[i] * i_samples[i];
       instpower[i] = ((uint32_t)rawvalue* 5000) / 1024;
    }

volatile uint8_t current_RMS(uint16_t i_samples[samples]){
	uint8_t i_RMS = 0;
	uint8_t diff = 0;
	for (uint8_t i = 0; i < samples; i++){
		diff = i_samples[i] - OFFSET;
		i_RMS += diff * diff;
	}
	return sqrt(i_RMS/samples);
}

volatile uint8_t voltage_RMS(uint16_t v_samples[samples]){
	uint8_t v_RMS = 0;
	uint8_t diff = 0;
	for (uint8_t i = 0; i < samples; i++){
		diff = v_samples[i] - OFFSET;
		v_RMS += diff * diff;
	}
	return sqrt(v_RMS/samples);
}
 
}
