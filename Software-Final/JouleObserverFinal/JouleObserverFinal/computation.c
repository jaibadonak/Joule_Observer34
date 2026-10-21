/*
 * computation.c
 *
 * Created: 17/10/2026 4:42:26 pm
 *  Author: Team_34
 *
 * The file is in the same order main.c uses it.
 *   1. Timer1 time keeping
 *   2. Find the whole cycles in a block
 *   3. DC offset
 *   4. RMS (voltage and current)
 *   5. Peak current
 *   6. Real power
 *   7. Apparent power, power factor, frequency (only for the optional $P line)
 *   8. Energy
 */


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "adc.h"
#include "computation.h"

// Current channel sign, set once in calibration.h (N4). Applied wherever a centred current is formed.
#if I_CHANNEL_INVERTED
#define I_SIGN (-1L)
#else
#define I_SIGN (1L)
#endif

static uint32_t energy_acc = 0;             // energy not yet added to the total, in mW x ticks
static uint32_t energy_total_mWh = 0;       // energy so far
static volatile uint16_t timer1_overflows = 0;
//block is pretty much where the samples are, the address of ethier one the samples



//============================ 1. TIMER1 TIME KEEPING ============================//
// 1 tick = 32 us; extended counter wraps after about 38 hours.
void calcs_init(void) {
	TCCR1A = 0;                             // normal mode
	TCCR1B = (1<<CS11) | (1<<CS10);       // clk/64
    TIMSK1 |= (1<<TOIE1);
}

// Extend Timer1 so a complete waveform transmission can span a hardware wrap.
ISR(TIMER1_OVF_vect) { timer1_overflows++; }

// Time now, in ticks. Overflows are the top 16 bits, TCNT1 the bottom 16.
uint32_t calcs_ticks(void) {
    uint16_t high, low;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        high = timer1_overflows;
        low = TCNT1;
        if (TIFR1 & (1<<TOV1)) { high++; low = TCNT1; }
    }
    return ((uint32_t)high << 16) | low;
}



//============================ 2. FIND THE WHOLE CYCLES ============================//
// A block is 10.4 cycles. The 0.4 leftover biases every average, so everything below
// only uses the samples between the first and last rising crossing (N1, N2).
typedef struct {
    uint16_t first, last;
    uint8_t crossings;
} cycle_window_t;

//private function.
//First and last rising crossing in the block, so everything between them is a whole number of cycles.
//Positions are x256 (Q8) so they can land between samples.
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

//private function.
//Turns the crossing positions into array indexes. Use block[start] ... block[stop-1].
//+128U /256 rounds to the nearest sample, because position is given in X256
//If no clean cycles were found, the whole block is used (0 ... 199).
static void whole_cycle_range(volatile uint16_t *block, uint16_t *start, uint16_t *stop) {
    cycle_window_t window;
    *start = 0;
    *stop = samples;                                  // default, the whole block
    if (find_complete_cycles(block, &window)) {
        *start = (window.first + 128U) / 256U;        // nearest sample to first crossing
        *stop  = (window.last  + 128U) / 256U;        // nearest sample to last crossing
    }
}



//============================ 3. DC OFFSET ============================//
//      offset = ( block[start] + ... + block[stop-1] ) / n
// Returned x100 (42237 = 422.37 counts). Pass v_samples or i_samples.
uint32_t adc_cycle_offset_x100(volatile uint16_t *block) {
    uint16_t start, stop;
    whole_cycle_range(block, &start, &stop);
    uint16_t n = stop - start;                        // samples in the window

    uint32_t total = 0;                               // 32 bit, 200 samples of ~422 overflow 16 bits
    for (uint16_t k = start; k < stop; k++) total += block[k]; //within those set samples, add all the samples
    return (total * 100UL + n / 2) / n;               //return average DC_offset, + n/2 rounds
}



//============================ 4. RMS ============================//
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

//      RMS = sqrt( mean( (block[k] - offset)^2 ) )      k = start ... stop-1
// AC RMS in counts x100 (e.g. 31250 = 312.50 counts), before gain.
uint16_t adc_rms_counts_x100(volatile uint16_t *block) {
    // Step 1. which samples (whole cycles only)
    uint16_t start, stop;
    whole_cycle_range(block, &start, &stop);
    uint16_t n = stop - start;

    // Step 2. DC offset, x100
    int32_t offset_x100 = (int32_t)adc_cycle_offset_x100(block);

    // Step 3. square each centred sample and add them up. Both sides x100, so the square is x10000
    uint64_t sum_sq = 0;
    for (uint16_t k = start; k < stop; k++) {
        int32_t x = (int32_t)block[k] * 100L - offset_x100;// get rid offset
        sum_sq += (uint64_t)((int64_t)x * x); //square and add each number
    }

    // Step 4. mean, then square root
    uint64_t mean_sq = (sum_sq + n / 2) / n;          // (RMS x100) squared
    uint32_t root = square_root_64(mean_sq);
    return root + (mean_sq - (uint64_t)root * root > root);   // nearest 0.01 count
}

uint16_t adc_rms_counts_x10(volatile uint16_t *block) {
    return (adc_rms_counts_x100(block) + 5U) / 10U;
}

//      Vrms [mV] = RMS counts x V_MV_PER_COUNT
uint32_t calc_voltage_rms_mV(void) {
	uint16_t rms_x100 = adc_rms_counts_x100(v_samples);
	return ((uint32_t)rms_x100 * V_MV_PER_COUNT_X100 + 5000UL) / 10000UL;  // x100 x100 = x10000
}

//      Irms [mA] = RMS counts x I_MA_PER_COUNT
// Returned in hundredths of a mA so it can be shown with two decimals.
uint32_t calc_current_rms_mA_x100(void) {
	uint16_t rms_x100 = adc_rms_counts_x100(i_samples);
	return ((uint32_t)rms_x100 * I_MA_PER_COUNT_X1000 + 500UL) / 1000UL;   // x100 x1000 / 1000 = x100
}



//============================ 5. PEAK CURRENT ============================//
//      Ipeak [mA] = biggest |i_samples[k] - offset| x I_MA_PER_COUNT
uint32_t calc_current_peak_mA(void) {
	uint32_t offset = adc_cycle_offset_x100(i_samples);
	int32_t peak = 0;

	for (uint16_t k = 0; k < samples; k++) {
		int32_t x = (int32_t)i_samples[k]*100L - (int32_t)offset;
		if (x < 0) x = -x;
		if (x > peak) peak = x;
	}

	return ((uint64_t)peak * I_MA_PER_COUNT_X1000 + 50000UL) / 100000UL;  // x100 x1000 = x100000
}



//============================ 6. REAL POWER ============================//
//      P = mean( (v[k] - offset_v) x (i[k] - offset_i) ) x P_MW_PER_COUNT2
// Over whole VOLTAGE cycles only. v[k] and i[k] are at the same phase (both blocks start
// on the VZC edge), so the voltage window is used for the current too (N3).
uint32_t calc_power_mW(void) {
    // Step 1. which samples, taken from the voltage block
    uint16_t start, stop;
    whole_cycle_range(v_samples, &start, &stop);
    uint16_t n = stop - start;

    // Step 2. both DC offsets over that same window, x100
    uint32_t total_v = 0, total_i = 0;
    for (uint16_t k = start; k < stop; k++) {
        total_v += v_samples[k];
        total_i += i_samples[k];
    }
    int32_t offset_v = (total_v * 100UL + n / 2) / n;
    int32_t offset_i = (total_i * 100UL + n / 2) / n;

    // Step 3. multiply centred v and i, add them up. Both x100, so each product is x10000
    int64_t sum_xy = 0;
    for (uint16_t k = start; k < stop; k++) {
        int32_t xv = (int32_t)v_samples[k] * 100L - offset_v;
        int32_t xi = I_SIGN * ((int32_t)i_samples[k] * 100L - offset_i);   // sign set in calibration.h
        sum_xy += (int64_t)xv * xi;
    }

    // Step 4. mean, keeping two decimals of counts^2 (N5)
    int64_t mean_xy_x100 = sum_xy / ((int32_t)n * 100L);   // counts^2 x100
    // Negative means power flowing back, impossible for a passive load.
    // Most likely I_CHANNEL_INVERTED is set wrong, so show 0 rather than hide it.
    if (mean_xy_x100 < 0) return 0;

    // Step 5. counts^2 to mW. x100 here times x10000 in the gain = x10^6
    return ((uint64_t)mean_xy_x100 * P_MW_PER_COUNT2_X10000 + 500000UL) / 1000000UL;
}



//============================ 7. S, PF, FREQUENCY (optional $P line) ============================//
//      S [mVA] = Vrms x Irms
uint32_t calc_apparent_power_mVA(uint32_t voltage_mV, uint32_t current_mA_x100) {
    return ((uint64_t)voltage_mV * current_mA_x100 + 50000UL) / 100000UL;
}

//      PF = P / S, sent in thousandths. -1 means unavailable.
// A small overshoot near PF = 1 is rounding/noise, so up to 2% over is clamped to 1.000 (N6).
int16_t calc_power_factor_x1000(uint32_t power_mW, uint32_t apparent_mVA) {
    if (!apparent_mVA) return -1;
    if (power_mW > apparent_mVA) {
        if (power_mW > apparent_mVA + apparent_mVA / 50) return -1;  // more than 2% over, something is wrong
        return 1000;                                                 // rounding overshoot, PF = 1.000
    }
    return ((uint64_t)power_mW * 1000UL + apparent_mVA/2) / apparent_mVA;
}

//      f [mHz] = whole cycles / time they took
// Time = (last - first) x 104 us, positions are x256. Zero means no reliable cycles.
uint32_t calc_frequency_mHz(void) {
    cycle_window_t window;
    if (!find_complete_cycles(v_samples, &window)) return 0;
    uint32_t elapsed = (uint32_t)(window.last - window.first) * ADC_SAMPLE_US;
    uint64_t numerator = (uint64_t)(window.crossings - 1) * 256UL * 1000000000ULL;
    return (numerator + elapsed/2) / elapsed;
}



//============================ 8. ENERGY ============================//
//      E = P x time. Builds up in mW x ticks, and every TICKS_PER_MWH adds 1 mWh.
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



//----------------------------NOTES-----------------------------------
//N1:Crossing level is the midpoint. It's stored doubled (lo + hi instead of (lo + hi)/2) so no half-count is lost to integer division. Comparisons later are written as 2*a vs level2 to match.
//Any level works, as long as it's the same level every cycle. Rising crossing to rising crossing is exactly one period regardless.
//low_gate is hysteresis. A crossing only counts if the signal dipped below 3/8 since the last one. Noise wobbling around the midpoint can't count as several crossings.
//
//N2:Why whole cycles. Averaging over whole cycles means every high has a matching low, so the AC part cancels
//and only the true DC (or true power) is left. The order is always
//find_complete_cycles (where) -> whole_cycle_range (which indexes) -> average/square/multiply over those indexes.
//
//N3:Power works out its own offsets instead of calling adc_cycle_offset_x100, because that would find the
//current's own window. The current lags, so its window is at different indexes, and v[k] would no longer pair with i[k].
//
//N4:I_SIGN comes from I_CHANNEL_INVERTED in calibration.h. The current diff amp output is upside down relative to
//the load current, so the centred current in power is multiplied by I_SIGN. RMS and peak square or abs the value, so they don't need it.
//
//N5:Dividing sum_xy by n x 100 instead of n x 10000 keeps two decimals of counts^2.
//That is why the final divide is by 10^6 (x100 from here times x10000 from P_MW_PER_COUNT2_X10000).
//
//N6:With PF near 1, rounding and noise can push P a hair above S. Up to 2% over is treated as PF = 1.000.
//More than that means something is genuinely wrong, so -1 (unavailable) is still returned.
