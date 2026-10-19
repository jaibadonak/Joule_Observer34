#ifndef JOULE_CALIBRATION_H
#define JOULE_CALIBRATION_H

// RMS gain calibration. Rebuild and flash the AVR to apply.
#define ADC_REFERENCE_MV 4970UL
#define V_MV_PER_COUNT_X100 10817UL
#define I_MA_PER_COUNT_X1000 3817UL

#if ADC_REFERENCE_MV < 1000 || ADC_REFERENCE_MV > 5500
#error "ADC reference out of range"
#endif
#if V_MV_PER_COUNT_X100 < 1 || V_MV_PER_COUNT_X100 > 60000 || I_MA_PER_COUNT_X1000 < 1 || I_MA_PER_COUNT_X1000 > 60000
#error "Calibration gain out of range"
#endif
#define P_MW_PER_COUNT2_X10000 ((V_MV_PER_COUNT_X100 * I_MA_PER_COUNT_X1000 + 5000UL) / 10000UL)

#endif
