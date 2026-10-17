/*
 * calcs.h
 *
 * Created: 15/10/2026 12:24:55 pm
 *  Author: Admin
 *


#ifndef CALCS_H_
#define CALCS_H_

volatile uint8_t voltage_RMS(uint16_t voltage[200]);
volatile uint8_t current_RMS(uint16_t current[200]);
volatile void compute_instPower(uint16_t voltage[200], uint16_t current[200]);




#endif /* CALCS_H_ */

/*
 * calcs.h
 *
 * Created: 15/10/2026 12:24:55 pm
 *  Author: Admin
 *
 * Turns the ADC blocks (v_samples[], i_samples[]) into Vrms, peak current,
 * real power and energy. Integer maths only, in milli-units like the UART.
 */


#ifndef CALCS_H_
#define CALCS_H_

#include <stdint.h>

//---------------------------- Scaling (from analogue/MASTER_SCHEMATIC_EE209.asc) ----------------------------//
// Check these against the real board and adjust if the readings are off.
//
// ADC: 5 V reference, 10 bit, so 1 count = 5000 / 1024 = 4.8828 mV.
//
// Offset: both channels sit on VREF = 5 x 3.3 / (4.7 + 3.3) = 2.0625 V,
//         2.0625 / 5 x 1024 = 422 counts. Subtract it to get the signed wave.
#define ADC_OFFSET_COUNTS        422
//
// Voltage: divider 4.7k / 104.7k = 0.04489, then a difference amp of gain
//          2 x 10k / (10k + 5.6k + 100k||4.7k) = 0.9956, so 0.04469 V per V.
//          4.8828 mV / 0.04469 = 109.26 mV of mains voltage per count.
#define V_MV_PER_COUNT_X100      10926      // 109.26 mV per count, x100
//
// Current: 0.5 ohm shunt, difference amp gain 68k / 27k = 2.5185, so 1.2593 mV per mA.
//          4.8828 mV / 1.2593 = 3.877 mA of load current per count.
#define I_MA_PER_COUNT_X1000     3877       // 3.877 mA per count, x1000
//
// Power: one count of V times one count of I = 109.26 mV x 3.877 mA = 0.4236 mW.
#define P_MW_PER_COUNT2_X10000   4236       // 0.4236 mW per count x count, x10000

//---------------------------- Energy time base ----------------------------//
// Timer1 free runs at 2 MHz / 64 = 31250 Hz (one tick = 32 us).
// One milliwatt-hour is 3600 s x 31250 ticks/s = 112,500,000 mW x ticks.
#define TICKS_PER_MWH            112500000UL

// Start Timer1 (free running). Read the time with TCNT1.
void calcs_init(void);

// Work on the blocks in v_samples[] and i_samples[]
uint32_t calc_voltage_rms_mV(void);      // Vrms of the mains, in mV
uint32_t calc_current_peak_mA(void);     // peak load current, in mA
uint32_t calc_power_mW(void);            // real power, mean of v x i, in mW

// Energy: add power_mW for dt_ticks Timer1 ticks, read the total in mWh
void     energy_add(uint32_t power_mW, uint16_t dt_ticks);
uint32_t energy_mWh(void);

#endif /* CALCS_H_ */