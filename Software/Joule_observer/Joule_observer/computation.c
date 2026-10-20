/*
 * computation.c
 *
 * Created: 17/10/2026 4:42:26 pm
 *  Author: Admin
 */ 


#include <avr/io.h>
#include <math.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "adc.h"
#include "computation.h"

static uint32_t energy_acc = 0;             // energy not yet added to the total, in mW x ticks
static uint32_t energy_total_mWh = 0;       // energy so far
static volatile uint16_t timer1_overflows = 0;


// Average of one block = its DC offset. The total is 32 bit because 200 samples of about 422
// add up to more than a 16 bit number can hold. Works out when called, so it uses the block
// that was just captured (it can't run at start up, the samples don't exist yet).
uint16_t adc_offset(volatile uint16_t *block) {
	uint32_t total = 0;

	for (uint16_t k = 0; k < samples; k++) {
		total = total + block[k];
	}

	return (uint16_t)((total + samples / 2) / samples);       // + samples/2 rounds to the nearest count
}

// A partial last cycle biases a plain block mean, creating alternating V*I peaks.
// Integrate from the first to last qualified rising crossing of the same level.
// Q8 sample positions preserve fractional boundaries without floating point.
typedef struct {
    uint16_t first, last;
    uint8_t crossings;
} cycle_window_t;

static uint8_t find_complete_cycles(volatile uint16_t *block, cycle_window_t *window) {
    uint16_t lo = 1023, hi = 0;
    for (uint16_t k = 0; k < samples; k++) {
        if (block[k] < lo) lo = block[k];
        if (block[k] > hi) hi = block[k];
    }
    uint16_t range = hi - lo;
    if (range < 16) return 0;
    uint16_t level2 = lo + hi;
    uint16_t low_gate = lo + range * 3U / 8U;
    uint16_t first = 0, last = 0, shortest = 65535, longest = 0;
    uint8_t crossings = 0, below = block[0] <= low_gate;
    for (uint16_t k = 1; k < samples; k++) {
        uint16_t a = block[k - 1], b = block[k];
        if (b <= low_gate) below = 1;
        if (below && 2U*a <= level2 && 2U*b > level2) {
            uint16_t position = (uint16_t)((k - 1)*256UL
                + (uint32_t)(level2 - 2U*a)*128UL/(b - a));
            if (!crossings) first = position;
            else {
                uint16_t period = position - last;
                if (period < shortest) shortest = period;
                if (period > longest) longest = period;
            }
            last = position; crossings++; below = 0;
        }
    }
    // Reject weak/short/noisy records instead of inventing a periodic correction.
    if (crossings < 2 || shortest < 8U*256U || longest-shortest > longest/5U)
        return 0;
    window->first = first; window->last = last; window->crossings = crossings;
    return 1;
}

uint32_t adc_cycle_offset_x100(volatile uint16_t *block) {
    cycle_window_t window;
    if (!find_complete_cycles(block, &window)) {
        uint32_t total = 0;
        for (uint16_t k = 0; k < samples; k++) total += block[k];
        return (total * 100UL + samples/2) / samples;
    }
    uint16_t first = window.first, last = window.last;
    uint32_t area = 0;
    for (uint16_t k = first/256U; k <= last/256U && k < samples-1; k++) {
        uint16_t start = k == first/256U ? first%256U : 0;
        uint16_t end = k == last/256U ? last%256U : 256;
        int32_t a = (int32_t)block[k]*256L;
        int32_t slope = (int32_t)block[k + 1] - block[k];
        // Trapezoid area in ADC-count * Q8-sample units. Bounded below 2^27.
        area += (uint32_t)(((2*a + slope*(start + end))*(end-start) + 256L)/512L);
    }
    uint16_t duration = last - first;
    return (area/duration)*100UL + ((area%duration)*100UL + duration/2U)/duration;
}


// Extend Timer1 so a complete waveform transmission can span a hardware wrap.
ISR(TIMER1_OVF_vect) { timer1_overflows++; }

uint32_t calcs_ticks(void) {
    uint16_t high, low;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        high = timer1_overflows;
        low = TCNT1;
        if (TIFR1 & (1<<TOV1)) { high++; low = TCNT1; }
    }
    return ((uint32_t)high << 16) | low;
}

// 1 tick = 32 us; extended counter wraps after about 38 hours.
void calcs_init(void) {
	TCCR1A = 0;                             // normal mode
	TCCR1B = (1<<CS11) | (1<<CS10);       // clk/64
    TIMSK1 |= (1<<TOIE1);
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

// Keep fractional mean-square counts before the square root. Dividing first
// rounded any block with <1 count squared to zero, hiding small AC signals.
uint16_t adc_rms_counts_x100(volatile uint16_t *block) {
	uint16_t offset = adc_offset(block);
	uint32_t sum_sq = 0;
	int32_t sum_diff = 0;

	for (uint16_t k = 0; k < samples; k++) {
		int32_t x = (int32_t)block[k] - offset;
		sum_sq = sum_sq + (uint32_t)(x * x);
		sum_diff += x;
	}

	// Split before multiplying to preserve precision without overflowing 32 bits.
	// For 10-bit samples centred on their mean, variance is <= 1023^2/4.
	// Scaling by 10000 therefore remains within uint32_t.
	uint32_t mean_sq_x10000 = (sum_sq / samples) * 10000UL
	                       + ((sum_sq % samples) * 10000UL) / samples;
	// Remove the fractional DC offset left by rounding adc_offset().
	uint32_t dc_sq_x10000 = (uint32_t)(sum_diff * sum_diff) * 10000UL
	                      / ((uint32_t)samples * samples);
	uint32_t variance = mean_sq_x10000 - dc_sq_x10000;
	uint16_t root = square_root(variance);
	return root + (variance - (uint32_t)root * root > root); // nearest 0.01 count
}

uint16_t adc_rms_counts_x10(volatile uint16_t *block) {
    return (adc_rms_counts_x100(block) + 5U) / 10U;
}

// Vrms, with each channel's own measured DC offset removed.
uint32_t calc_voltage_rms_mV(void) {
	uint16_t rms_x100 = adc_rms_counts_x100(v_samples);
	return ((uint32_t)rms_x100 * V_MV_PER_COUNT_X100 + 5000UL) / 10000UL;
}

// Irms = square root of the mean of x squared, same method as the voltage.
// Returned in hundredths of a mA so it can be shown with two decimals.
uint32_t calc_current_rms_mA_x100(void) {
	uint16_t rms_x100 = adc_rms_counts_x100(i_samples);
	return ((uint32_t)rms_x100 * I_MA_PER_COUNT_X1000 + 500UL) / 1000UL;
}

// Peak current = biggest |x| in the block
uint32_t calc_current_peak_mA(void) {
	uint32_t offset = adc_cycle_offset_x100(i_samples);
	int32_t peak = 0;

	for (uint16_t k = 0; k < samples; k++) {
		int32_t x = (int32_t)i_samples[k]*100L - (int32_t)offset;
		if (x < 0) x = -x;
		if (x > peak) peak = x;
	}

	return ((uint64_t)peak * I_MA_PER_COUNT_X1000 + 50000UL) / 100000UL;
}

// Real power = mean of v x i. v[k] and i[k] are the same point in the cycle
// because both blocks start on the voltage zero crossing.
uint32_t calc_power_mW(void) {
	uint32_t offset_v = adc_cycle_offset_x100(v_samples);
	uint32_t offset_i = adc_cycle_offset_x100(i_samples);
	int64_t sum_xy = 0;

	for (uint16_t k = 0; k < samples; k++) {
		int32_t xv = (int32_t)v_samples[k]*100L - (int32_t)offset_v;
		int32_t xi = (int32_t)i_samples[k]*100L - (int32_t)offset_i;
		sum_xy += (int64_t)xv * xi;
	}

    int32_t mean_xy = sum_xy / ((int32_t)samples * 10000L);
    // Existing passive-load convention: compensate an inverted current channel.
    uint32_t magnitude = mean_xy < 0 ? (uint32_t)(-mean_xy) : (uint32_t)mean_xy;
    // Split the scale multiply to avoid signed overflow on clipped ADC inputs.
    return (magnitude / 10000UL) * P_MW_PER_COUNT2_X10000
         + ((magnitude % 10000UL) * P_MW_PER_COUNT2_X10000) / 10000UL;
}

// Add power x time to the running total
void energy_add(uint32_t power_mW, uint32_t dt_ticks) {
	if (power_mW > 60000UL) power_mW = 60000UL;     // keeps the multiply below inside 32 bits

    // Bound each product AND the addition to the previous remainder.
    while (dt_ticks) {
        uint16_t step = dt_ticks > 1000 ? 1000 : dt_ticks;
        energy_acc += power_mW * step;
        dt_ticks -= step;
        while (energy_acc >= TICKS_PER_MWH) {
            energy_acc -= TICKS_PER_MWH;
            energy_total_mWh++;
        }
    }
}

uint32_t energy_mWh(void) {
	return energy_total_mWh;
}


// Vrms[mV] * Irms[hundredths of mA] -> apparent power in milli-VA.
uint32_t calc_apparent_power_mVA(uint32_t voltage_mV, uint32_t current_mA_x100) {
    return ((uint64_t)voltage_mV * current_mA_x100 + 50000UL) / 100000UL;
}

// PF is P/S, sent in thousandths. -1 means unavailable (zero S or invalid ratio).
int16_t calc_power_factor_x1000(uint32_t power_mW, uint32_t apparent_mVA) {
    if (!apparent_mVA || power_mW > apparent_mVA) return -1;
    return ((uint64_t)power_mW * 1000UL + apparent_mVA/2) / apparent_mVA;
}

// Frequency from interpolated voltage crossings. Zero means no reliable cycles.
uint32_t calc_frequency_mHz(void) {
    cycle_window_t window;
    if (!find_complete_cycles(v_samples, &window)) return 0;
    uint32_t elapsed = (uint32_t)(window.last - window.first) * ADC_SAMPLE_US;
    uint64_t numerator = (uint64_t)(window.crossings - 1) * 256UL * 1000000000ULL;
    return (numerator + elapsed/2) / elapsed;
}

static int32_t rounded_signed_divide(int64_t value, int32_t divisor) {
    return (value + (value < 0 ? -divisor/2 : divisor/2)) / divisor;
}

int32_t scale_adc_sample(uint16_t raw, uint32_t dc_x100, uint32_t gain, int32_t divisor) {
    int64_t centred = (int32_t)raw * 100L - (int32_t)dc_x100;
    return rounded_signed_divide(centred * gain, 100L * divisor);
}

void calc_waveform_sample(uint16_t index, uint32_t voltage_dc, uint32_t current_dc,
                          waveform_sample_t *value) {
    value->voltage_mV = scale_adc_sample(v_samples[index], voltage_dc, V_MV_PER_COUNT_X100, 100);
    value->current_mA = scale_adc_sample(i_samples[index], current_dc, I_MA_PER_COUNT_X1000, 1000);
    int32_t voltage = (int32_t)v_samples[index]*100L - (int32_t)voltage_dc;
    int32_t current = (int32_t)i_samples[index]*100L - (int32_t)current_dc;
    // Use unrounded ADC values for V*I; retain the sign of instantaneous power.
    value->power_mW = rounded_signed_divide((int64_t)voltage * current * P_MW_PER_COUNT2_X10000,
                                           100000000L);
}

void capture_stats_add(capture_stats_t *stats, int32_t value) {
    if (!stats->count || value < stats->minimum) stats->minimum = value;
    if (!stats->count || value > stats->maximum) stats->maximum = value;
    stats->sum += value;
    stats->sum_squares += (int64_t)value * value;
    stats->count++;
}

static uint32_t square_root_64(uint64_t value) {
    uint64_t result = 0, bit = 1ULL << 62;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= result + bit) { value -= result + bit; result = (result >> 1) + bit; }
        else result >>= 1;
        bit >>= 2;
    }
    return (uint32_t)result;
}

int32_t capture_stats_mean(const capture_stats_t *stats) {
    return stats->count ? rounded_signed_divide(stats->sum, stats->count) : 0;
}

uint32_t capture_stats_rms(const capture_stats_t *stats) {
    return stats->count ? square_root_64(stats->sum_squares / stats->count) : 0;
}
