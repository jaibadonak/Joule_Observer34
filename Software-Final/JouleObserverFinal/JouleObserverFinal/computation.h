/*
 * computation.h
 *
 * Created: 20/10/2026 1:38:38 pm
 *  Author: Admin
 */


#ifndef COMPUTATION_H_
#define COMPUTATION_H_

#include <stdint.h>
#include "config.h"
#include "calibration.h"

// Voltage: divider 4.7k / (100k + 4.7k) = 0.04489, then a difference amp of gain
//          2 x 10k / (10k + 5.6k + 100k||4.7k) = 0.9956, so 0.04469 V per V.
//          4.8828 mV / 0.04469 = 109.26 mV of mains voltage per count.
//--------------------------Defined in calibration---------------------------------//

// Current: ~0.5 ohm shunt, difference amp gain 68k / 27k = 2.5185, so 1.2593 mV per mA.
//          4.8828 mV / 1.2593 = 3.877 mA of load current per count.
//--------------------------Defined in calibration---------------------------------//

// Power: one count of V times one count of I = 109.26 mV x 3.877 mA = 0.4236 mW.
//--------------------------Defined in calibration---------------------------------//

// Timer1 free runs at 2 MHz / 64 = 31250 Hz (one tick = 32 us).
// One milliwatt-hour is 3600 s x 31250 ticks/s = 112,500,000 mW x ticks.
#define TICKS_PER_MWH (3600UL * TIMER1_TICKS_PER_SECOND)

// DC offset of one block of samples, x100 counts (42237 = 422.37 counts).
// Averaged over whole cycles only, so the partial last cycle can't bias it.
// Falls back to the plain block average if no clean cycles are found.
// Pass v_samples or i_samples. Each channel gets its own offset.
uint32_t adc_cycle_offset_x100(volatile uint16_t *block);
uint16_t adc_rms_counts_x10(volatile uint16_t *block); // raw AC RMS before gain calibration
uint16_t adc_rms_counts_x100(volatile uint16_t *block); // rounded hundredths of a count

// Start Timer1 (free running). Read the time with calcs_ticks(), not TCNT1 (which wraps every 2.1 s).
void calcs_init(void);
uint32_t calcs_ticks(void); // atomic extended Timer1 snapshot, handles pending overflow

// Work on the blocks in v_samples[] and i_samples[]
uint32_t calc_voltage_rms_mV(void);      // Vrms of the mains, in mV
uint32_t calc_current_rms_mA_x100(void); // RMS load current, in hundredths of a mA (53539 = 535.39 mA)
uint32_t calc_current_peak_mA(void);     // peak load current, averaged over whole cycles, in mA
uint32_t calc_power_mW(void);            // real power, mean of v x i, in mW

// Energy: add power_mW for dt_ticks Timer1 ticks, read the total in mWh
void     energy_add(uint32_t power_mW, uint32_t dt_ticks);
uint32_t energy_mWh(void);

#define ADC_SAMPLE_US 104UL
uint32_t calc_apparent_power_mVA(uint32_t voltage_mV, uint32_t current_mA_x100);
int16_t calc_power_factor_x1000(uint32_t power_mW, uint32_t apparent_mVA); // -1 unavailable
uint32_t calc_frequency_mHz(void); // 0 unavailable

typedef struct {
	int32_t voltage_mV, current_mA, power_mW;
} waveform_sample_t;

typedef struct {
	int32_t minimum, maximum;
	int64_t sum;
	uint64_t sum_squares;
	uint16_t count;
} capture_stats_t;

int32_t scale_adc_sample(uint16_t raw, uint32_t dc_x100, uint32_t gain, int32_t divisor);
void calc_waveform_sample(uint16_t index, uint32_t voltage_dc, uint32_t current_dc,
waveform_sample_t *value);
void capture_stats_add(capture_stats_t *stats, int32_t value);
int32_t capture_stats_mean(const capture_stats_t *stats);
uint32_t capture_stats_rms(const capture_stats_t *stats);


#endif /* COMPUTATION_H_ */
