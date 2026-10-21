// Run the final project's calculation code in the AVR simulator.
#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>
#include "../JouleObserverFinal/JouleObserverFinal/calibration.h"
#ifdef TEST_INVERT_CURRENT
#undef I_CHANNEL_INVERTED
#define I_CHANNEL_INVERTED 1
#endif
#undef ISR
#define ISR(vector, ...) void vector(void)
#include "../JouleObserverFinal/JouleObserverFinal/computation.c"

volatile uint16_t v_samples[samples], i_samples[samples];
volatile uint16_t test_failures = 0;
volatile uint16_t failure_lines[16];
#define CHECK(condition) do { if (!(condition)) { if (test_failures < 16) failure_lines[test_failures] = __LINE__; test_failures++; } } while (0)

void __attribute__((noinline)) test_finished(void) { __asm__ __volatile__("nop"); }

int main(void) {
    capture_stats_t stats = {0};
    capture_stats_add(&stats, -3000);
    capture_stats_add(&stats, 4000);
    CHECK(stats.minimum == -3000 && stats.maximum == 4000);
    CHECK(capture_stats_mean(&stats) == 500 && capture_stats_rms(&stats) == 3535);

    // Fractional DC offsets, both signs and the whole 10-bit input range.
    // Floating-point references are independent of the production fixed-point rounding.
    for (uint16_t raw = 0; raw <= 1023; raw += 3) {
        v_samples[0] = raw;
        i_samples[0] = 1023 - raw;
        waveform_sample_t result;
        calc_waveform_sample(0, 42237, 41763, &result);
        double voltage = raw - 422.37;
        double current = I_SIGN * (1023 - raw - 417.63);
        CHECK(fabs(result.voltage_mV - voltage * V_MV_PER_COUNT_X100 / 100.0) < 0.6);
        CHECK(fabs(result.current_mA - current * I_MA_PER_COUNT_X1000 / 1000.0) < 0.6);
        CHECK(fabs(result.power_mW - voltage * current * P_MW_PER_COUNT2_X10000 / 10000.0) < 0.7);
    }

    // Stable 500 Hz sine, with an inductive lag. Exercise the unchanged cycle/RMS/power code.
    for (uint16_t k = 0; k < samples; k++) {
        double phase = 6.283185307 * 500 * k * 0.000104;
        v_samples[k] = 422 + lround(180 * sin(phase));
        i_samples[k] = 418 + lround(I_SIGN * 120 * sin(phase - 0.6));
    }
    double voltage = 180 / sqrt(2.0) * V_MV_PER_COUNT_X100 / 100.0;
    double current = 120 / sqrt(2.0) * I_MA_PER_COUNT_X1000 / 1000.0;
    double power = voltage * current * cos(0.6) / 1000.0;
    CHECK(fabs(calc_voltage_rms_mV() - voltage) < voltage * 0.01);
    CHECK(fabs(calc_current_rms_mA_x100() / 100.0 - current) < current * 0.01);
    CHECK(fabs(calc_power_mW() - power) < power * 0.02);
    CHECK(fabs((double)calc_frequency_mHz() - 500000) < 200);
    CHECK(calc_apparent_power_mVA(14000, 40000) == 5600);
    CHECK(calc_power_factor_x1000(2800, 5600) == 500);
    energy_add(3600, TIMER1_TICKS_PER_SECOND);
    CHECK(energy_mWh() == 1);
    test_finished();
    return 0;
}
