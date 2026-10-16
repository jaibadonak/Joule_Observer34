/*
 * calcs.c
 *
 * Created: 15/10/2026 12:25:14 pm
 *  Author: Admin
 */ 
#define OFFSET 2.0625
volatile uint16_t instpower[samples];
#include "adc.h"
void compute_instPower() {
    for(int_t i = 0; i <= samples; i++) {
        uint32_t rawvalue =  v_samples[i] * i_samples[i];
       instpower = ((uint32_t)rawvalue* 5000) / 1024
    }

 
}


