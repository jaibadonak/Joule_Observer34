/*
 * computation.c
 *
 * Created: 17/10/2026 4:42:26 pm
 *  Author: Team_34
 */ 


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "adc.h"
#include "computation.h"

// Current channel sign, set once in calibration.h (N3). Applied wherever a centred current is formed.
#if I_CHANNEL_INVERTED
#define I_SIGN (-1L)
#else
#define I_SIGN (1L)
#endif

static uint32_t energy_acc = 0;             // energy not yet added to the total, in mW x ticks
static uint32_t energy_total_mWh = 0;       // energy so far
static volatile uint16_t timer1_overflows = 0;
//block is pretty much where the samples are, the address of ethier one the samples


// A partial last cycle biases a plain block mean, creating alternating V*I peaks.
// Average from the first to last qualified rising crossing of the same level.
// Q8 sample positions preserve fractional boundaries without floating point.
typedef struct {
    uint16_t first, last;
    uint8_t crossings;
} cycle_window_t;
//private function.
//First and last rising crossing in the block, so everything between them is a whole number of cycles. 
//Then the DC average is taken only over that window.
static uint8_t find_complete_cycles(volatile uint16_t *block, cycle_window_t *window) {
    uint16_t lo = 1023, hi = 0; //smallest and largest sample
	//finding the biggest count and smallest count/sample
    for (uint16_t k = 0; k < samples; k++) {
        if (block[k] < lo) lo = block[k];
        if (block[k] > hi) hi = block[k];
    }
    uint16_t range = hi - lo;
    if (range < 16) return 0; //biggest and smallest vlaue range is less than 16, ignore.
    uint16_t level2 = lo + hi; // 2* midpoint of wave N1 - V_TH
	//software smitch trigger, this is the band
    uint16_t low_gate = lo + range * 3U / 8U; // 3/8 of the way up the wave. V_TL
    uint16_t first = 0, last = 0, shortest = 65535, longest = 0; //defining variables
    uint8_t crossings = 0, below = (block[0] <= low_gate); // below = whether fisrt sample is below the 3/8 or not, 1 = yes, 0 = no

	//scan for rising crossings
    for (uint16_t k = 1; k < samples; k++) {
        uint16_t a = block[k - 1], b = block[k];
        if (b <= low_gate) below = 1; //if sample is below VTL, below = 1 (think of trigger now low)
        if (below && 2U*a <= level2 && 2U*b > level2) { // if the signal has dipped below V_TL since the last crossing, the previous sample is <= V_TH and the current sample is higher than V_TH
			//there for dectects a rising crossing of V_TH
            uint16_t position = (uint16_t)((k - 1)*256UL
                + (uint32_t)(level2 - 2U*a)*128UL/(b - a));
			//record the crossing values
            if (!crossings) first = position;
            else {
                uint16_t period = position - last; //record WHOLE cycle 
                if (period < shortest) shortest = period; //shortest weve seen? remeber it
                if (period > longest) longest = period; //longest weve seen? remeber it
            }
            last = position; crossings++; below = 0;
        }
    }
    // Reject weak/short/noisy records instead of inventing a periodic correction.
    if (crossings < 2 || shortest < 8U*256U || longest-shortest > longest/5U)
        return 0; // fail
    window->first = first; window->last = last; window->crossings = crossings; //gives two sample quantites that represent whole cycles within samples. 
    return 1; //pass
}

//private function. N2
//Sample range start ... stop-1 that covers whole cycles, or the whole block if none were found.
//+128U /256 rounds to the nearest sample, because position is given in X256
static void whole_cycle_range(volatile uint16_t *block, uint16_t *start, uint16_t *stop) {
    cycle_window_t window;
    *start = 0;
    *stop = samples;                                  // default, the whole block
    if (find_complete_cycles(block, &window)) {
        *start = (window.first + 128U) / 256U;        // nearest sample to first crossing
        *stop  = (window.last  + 128U) / 256U;        // nearest sample to last crossing
    }
}

//private function.
//Average of block[start] ... block[stop-1], x100. The total is 32 bit because 200 samples
//of about 422 add up to more than a 16 bit number can hold.
static uint32_t window_mean_x100(volatile uint16_t *block, uint16_t start, uint16_t stop) {
    uint16_t n = stop - start;                        // samples in the window
    uint32_t total = 0;
    for (uint16_t k = start; k < stop; k++) total += block[k]; //within those set samples, add all the samples 
    return (total * 100UL + n / 2) / n;               // + n/2 rounds to the nearest 0.01 count
}

// DC offset over whole cycles, x100 (42237 = 422.37 counts).
// If no good cycles were found this is the ordinary block average.
uint32_t adc_cycle_offset_x100(volatile uint16_t *block) {
    uint16_t start, stop;
    whole_cycle_range(block, &start, &stop);
    return window_mean_x100(block, start, stop);      //return average DC_offset
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

// Square root of a 64-bit number, rounded down (no floating point on the AVR)
static uint32_t square_root_64(uint64_t value) {
    uint64_t result = 0, bit = 1ULL << 62;      // highest power of 4 that fits
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= result + bit) { value -= result + bit; result = (result >> 1) + bit; }
        else result >>= 1;
        bit >>= 2;
    }
    return (uint32_t)result;
}

// AC RMS in counts x100 (e.g. 31250 = 312.50 counts), before gain.
// Offset and squares both use only the whole-cycle window, so the
// partial last cycle can't bias either. Falls back to the whole block.
uint16_t adc_rms_counts_x100(volatile uint16_t *block) {
    uint16_t start, stop;
    whole_cycle_range(block, &start, &stop);
    uint16_t n = stop - start;

    // DC offset over the window, x100
    int32_t offset_x100 = (int32_t)window_mean_x100(block, start, stop);

    // Mean of (sample - offset)^2. Both sides x100, so the square is x10000
    uint64_t sum_sq = 0;
    for (uint16_t k = start; k < stop; k++) {
        int32_t x = (int32_t)block[k] * 100L - offset_x100;// get rid offset
        sum_sq += (uint64_t)((int64_t)x * x); //square and add each number
    }
    uint64_t mean_sq = (sum_sq + n / 2) / n;          // (RMS x100) squared
    uint32_t root = square_root_64(mean_sq);
    return root + (mean_sq - (uint64_t)root * root > root);   // nearest 0.01 count
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

//private function.
//Biggest |x| between two sample indexes (from ... to-1), x100 counts.
static uint32_t peak_x100(volatile uint16_t *block, uint32_t offset_x100, uint16_t from, uint16_t to) {
	int32_t peak = 0;

	for (uint16_t k = from; k < to; k++) {
		int32_t x = (int32_t)block[k]*100L - (int32_t)offset_x100;
		if (x < 0) x = -x;
		if (x > peak) peak = x;
	}
	return (uint32_t)peak;
}

// Peak current = average of each whole cycle's biggest |x|, so one noise spike
// can't set the reading. N4. Falls back to the biggest |x| in the whole block.
uint32_t calc_current_peak_mA(void) {
	uint32_t offset = adc_cycle_offset_x100(i_samples);
	cycle_window_t window;
	uint32_t peak;

	if (find_complete_cycles(i_samples, &window)) {
		uint8_t cycles = window.crossings - 1;               // whole cycles in the window
		uint32_t span = window.last - window.first;          // their total length, x256 samples
		uint32_t peak_sum = 0;
		for (uint8_t c = 0; c < cycles; c++) {
			uint16_t from = (window.first + span * c / cycles + 128U) / 256U;        // start of cycle c
			uint16_t to   = (window.first + span * (c + 1) / cycles + 128U) / 256U;  // start of cycle c+1
			peak_sum += peak_x100(i_samples, offset, from, to);
		}
		peak = (peak_sum + cycles / 2) / cycles;
	} else {
		peak = peak_x100(i_samples, offset, 0, samples);
	}

	return ((uint64_t)peak * I_MA_PER_COUNT_X1000 + 50000UL) / 100000UL;
}

// Real power = mean of v x i over whole voltage cycles only.
// v[k] and i[k] are at the same phase (both blocks start on the VZC edge),
// so the window found on the voltage block applies to the current block too.
uint32_t calc_power_mW(void) {
    uint16_t start, stop;
    whole_cycle_range(v_samples, &start, &stop);
    uint16_t n = stop - start;

    // DC offsets over the window, x100
    int32_t offset_v = (int32_t)window_mean_x100(v_samples, start, stop);
    int32_t offset_i = (int32_t)window_mean_x100(i_samples, start, stop);

    // Sum of centred v x i. Both x100, so each product is x10000
    int64_t sum_xy = 0;
    for (uint16_t k = start; k < stop; k++) {
        int32_t xv = (int32_t)v_samples[k] * 100L - offset_v;
        int32_t xi = I_SIGN * ((int32_t)i_samples[k] * 100L - offset_i);   // sign set in calibration.h
        sum_xy += (int64_t)xv * xi;
    }

    // Keep two decimals of counts^2 (N5), so the result isn't truncated to 0.41 mW steps.
    int64_t mean_xy_x100 = sum_xy / ((int32_t)n * 100L);   // counts^2 x100
    // Negative means power flowing back, impossible for a passive load.
    // Most likely I_CHANNEL_INVERTED is set wrong, so show 0 rather than hide it.
    if (mean_xy_x100 < 0) return 0;
    // counts^2 x100 times mW per count^2 x10000 = mW x10^6
    return ((uint64_t)mean_xy_x100 * P_MW_PER_COUNT2_X10000 + 500000UL) / 1000000UL;
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

// PF is P/S, sent in thousandths. -1 means unavailable (zero S, or P well above S).
// N6. A small overshoot near PF = 1 is rounding/noise, so it is clamped to 1.000.
int16_t calc_power_factor_x1000(uint32_t power_mW, uint32_t apparent_mVA) {
    if (!apparent_mVA) return -1;
    if (power_mW > apparent_mVA) {
        if (power_mW > apparent_mVA + apparent_mVA / 50) return -1;  // more than 2% over, something is wrong
        return 1000;                                                 // rounding overshoot, PF = 1.000
    }
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
    value->current_mA = I_SIGN * scale_adc_sample(i_samples[index], current_dc, I_MA_PER_COUNT_X1000, 1000);
    int32_t voltage = (int32_t)v_samples[index]*100L - (int32_t)voltage_dc;
    int32_t current = I_SIGN * ((int32_t)i_samples[index]*100L - (int32_t)current_dc);
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

int32_t capture_stats_mean(const capture_stats_t *stats) {
    return stats->count ? rounded_signed_divide(stats->sum, stats->count) : 0;
}

uint32_t capture_stats_rms(const capture_stats_t *stats) {
    return stats->count ? square_root_64(stats->sum_squares / stats->count) : 0;
}



//----------------------------NOTES-----------------------------------
//N1:Crossing level is the midpoint. It's stored doubled (lo + hi instead of (lo + hi)/2) so no half-count is lost to integer division. Comparisons later are written as 2*a vs level2 to match.
//Any level works, as long as it's the same level every cycle. Rising crossing to rising crossing is exactly one period regardless.
//low_gate is hysteresis. A crossing only counts if the signal dipped below 3/8 since the last one. Noise wobbling around the midpoint can't count as several crossings.
//
//N2:whole_cycle_range is the one place the whole-cycle window is worked out. The offset, RMS and power all call it,
//so they always agree on which samples count. Averaging over whole cycles means every high has a matching low,
//so the AC part cancels and only the true DC (or true power) is left.
//
//N3:I_SIGN comes from I_CHANNEL_INVERTED in calibration.h. The current diff amp output is upside down relative to
//the load current, so every centred current is multiplied by I_SIGN. Power, the waveform current and the
//instantaneous power then all come out the right way up. RMS and peak square or abs the value, so they don't need it.
//
//N4:Peak current used to be the single biggest sample, so one noise spike set it. Now each whole cycle gets its own
//biggest |x| and those are averaged. Cycle c runs from first + c x (span / cycles) to the start of the next.
//
//N5:Dividing sum_xy by n x 100 instead of n x 10000 keeps two decimals of counts^2.
//That is why the final divide is by 10^6 (x100 from here times x10000 from P_MW_PER_COUNT2_X10000).
//
//N6:With PF near 1, rounding and noise can push P a hair above S. Up to 2% over is treated as PF = 1.000.
//More than that means something is genuinely wrong, so -1 (unavailable) is still returned.
