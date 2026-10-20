#ifndef JOULE_CALIBRATION_H
#define JOULE_CALIBRATION_H

// RMS gain calibration. Rebuild and flash the AVR to apply.
//Since for voltage G_d = 4.7/4.7+100 and G_O = ~1
//Since for current Rs= 0.54ohms and G_O = 68/27
//N1
#define ADC_REFERENCE_MV 4970UL
#define V_MV_PER_COUNT_X100 10817UL
#define I_MA_PER_COUNT_X1000 3817UL
//Simple checks for peace of mind
#if ADC_REFERENCE_MV < 1000 || ADC_REFERENCE_MV > 5500
#error "ADC reference out of range"
#endif
//why 60,000? in calc_voltage_rms_mV, rmsx100 is uint16, therefor preventing overflow.
#if V_MV_PER_COUNT_X100 < 1 || V_MV_PER_COUNT_X100 > 60000 || I_MA_PER_COUNT_X1000 < 1 || I_MA_PER_COUNT_X1000 > 60000
#error "Calibration gain out of range"
#endif
//N2
#define P_MW_PER_COUNT2_X10000 ((V_MV_PER_COUNT_X100 * I_MA_PER_COUNT_X1000 + 5000UL) / 10000UL)

#endif

//------------------NOTES----------------
//
//N1: change these values to esstionally change the gain of the read voltage to the acutal input voltage.
//this allows for a swift and easy calibration when observerin errors. An example on how you would chaneg the numbers are:
//Meter says 14.50 V, board says 14.30 V. 10817 × (14.50 / 14.30) = 10968.
//ps: we need to scale up because integers cant hold decimals, without scaling you'd have to round.
//why current is 1000X scaled, is because a smaller number is more suspectable to rounding error,
//therefor needs a bigger scale to keep the same amount of sig figures.
//
//N2: the reason for + 5000 and /10000, mV * mA = uW, the x100 and x1000 scale as they multiply, and now are 10^5. Dividing by 10000 gives uWx10 = mW x 10^4
