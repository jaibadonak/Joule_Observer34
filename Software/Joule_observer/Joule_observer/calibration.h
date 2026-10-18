#ifndef JOULE_CALIBRATION_H
#define JOULE_CALIBRATION_H

// Edit these three values, or replace this file with the dashboard download.
// Rebuild and flash the AVR to apply. Gain calibration does not correct phase.
#define ADC_REFERENCE_MV          5000UL  // measured AVCC/AREF, used for scope comparisons
#define V_MV_PER_COUNT_X100      10926UL  // load mV per ADC count, multiplied by 100
#define I_MA_PER_COUNT_X1000      3877UL  // load mA per ADC count, multiplied by 1000

#if ADC_REFERENCE_MV < 1000 || ADC_REFERENCE_MV > 5500
#error "ADC_REFERENCE_MV must be between 1000 and 5500."
#endif
#if V_MV_PER_COUNT_X100 < 1 || V_MV_PER_COUNT_X100 > 60000 || I_MA_PER_COUNT_X1000 < 1 || I_MA_PER_COUNT_X1000 > 60000
#error "Calibration gain constants must be between 1 and 60000."
#endif
// Keep power and waveform scaling consistent with both calibrated channels.
#define P_MW_PER_COUNT2_X10000 ((V_MV_PER_COUNT_X100 * I_MA_PER_COUNT_X1000 + 5000UL) / 10000UL)

#endif
