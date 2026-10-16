/*
 * calcs.c
 *
 * Created: 15/10/2026 12:25:14 pm
 *  Author: Admin
 *
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
*/

/*
 * calcs.c
 *
 * Created: 15/10/2026 12:25:14 pm
 *  Author: Admin
 */ 

#include <avr/io.h>
#include <stdint.h>
#include "adc.h"
#include "calcs.h"

static uint32_t energy_acc = 0;             // energy not yet added to the total, in mW x ticks
static uint32_t energy_total_mWh = 0;       // energy so far


// Timer1 just counts, nothing else uses it. 1 tick = 32 us, wraps every 2.1 s.
void calcs_init(void) {
	TCCR1A = 0;                             // normal mode
	TCCR1B = (1<<CS11) | (1<<CS10);         // clk/64
}

// Square root of a 32-bit number, rounded down (no floating point on the AVR)
static uint16_t square_root(uint32_t n) {
	uint32_t result = 0;
	uint32_t bit = 1UL << 30;               // highest power of 4 that fits

	while (bit > n) {
		bit = bit >> 2;
	}
	while (bit != 0) {
		if (n >= result + bit) {
			n = n - (result + bit);
			result = (result >> 1) + bit;
		} else {
			result = result >> 1;
		}
		bit = bit >> 2;
	}
	return (uint16_t)result;
}

// Vrms = square root of the mean of x squared, where x = count - offset
uint32_t calc_voltage_rms_mV(void) {
	uint32_t sum_sq = 0;

	for (uint16_t k = 0; k < samples; k++) {
		int32_t x = (int32_t)v_samples[k] - ADC_OFFSET_COUNTS;
		sum_sq = sum_sq + (uint32_t)(x * x);
	}

	uint32_t mean_sq = sum_sq / samples;
	uint16_t rms_x10 = square_root(mean_sq * 100UL);        // RMS in tenths of a count
	return ((uint32_t)rms_x10 * V_MV_PER_COUNT_X100) / 1000UL;
}

// Peak current = biggest |x| in the block
uint32_t calc_current_peak_mA(void) {
	int32_t peak = 0;

	for (uint16_t k = 0; k < samples; k++) {
		int32_t x = (int32_t)i_samples[k] - ADC_OFFSET_COUNTS;
		if (x < 0) x = -x;
		if (x > peak) peak = x;
	}

	return ((uint32_t)peak * I_MA_PER_COUNT_X1000) / 1000UL;
}

// Real power = mean of v x i. v[k] and i[k] are the same point in the cycle
// because both blocks start on the voltage zero crossing.
uint32_t calc_power_mW(void) {
	int32_t sum_xy = 0;

	for (uint16_t k = 0; k < samples; k++) {
		int32_t xv = (int32_t)v_samples[k] - ADC_OFFSET_COUNTS;
		int32_t xi = (int32_t)i_samples[k] - ADC_OFFSET_COUNTS;
		sum_xy = sum_xy + xv * xi;
	}

	int32_t mean_xy = sum_xy / samples;
	int32_t power = (mean_xy * P_MW_PER_COUNT2_X10000) / 10000L;

	// A passive load can't give power back, so a negative answer only means
	// one channel is inverted. Flip it.
	if (power < 0) power = -power;
	return (uint32_t)power;
}

// Add power x time to the running total
void energy_add(uint32_t power_mW, uint16_t dt_ticks) {
	if (power_mW > 60000UL) power_mW = 60000UL;     // keeps the multiply below inside 32 bits

	energy_acc = energy_acc + power_mW * dt_ticks;
	while (energy_acc >= TICKS_PER_MWH) {
		energy_acc = energy_acc - TICKS_PER_MWH;
		energy_total_mWh++;
	}
}

uint32_t energy_mWh(void) {
	return energy_total_mWh;
}