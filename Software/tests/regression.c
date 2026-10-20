/* Run with avr-gdb's instruction simulator; no attached board required.
 * Include implementation files so the capture state and segment buffers can
 * be checked without adding test hooks to the production firmware.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
// Feed samples explicitly; model ISR bodies as ordinary calls. RETI would
// enable interrupts when the test calls them outside real interrupt entry.
#undef ISR
#define ISR(vector, ...) void vector(void)
static volatile uint16_t simulated_adc;
#undef ADC
#define ADC simulated_adc
#include "../Joule_observer/Joule_observer/adc.c"
#include "../Joule_observer/Joule_observer/computation.c"
#include "../Joule_observer/Joule_observer/display.c"
#include "../Joule_observer/Joule_observer/uart.c"
#include "../../Application/src/uart_protocol.h"

volatile uint16_t test_failures = 0;
volatile uint16_t failure_lines[16];
volatile uint16_t spec_cases = 0;
volatile uint32_t spec_voltage_error_mV = 0, spec_peak_error_mA_x100 = 0;
volatile uint32_t spec_power_error_mW = 0;
#define CHECK(condition) do { if (!(condition)) { if (test_failures<16) failure_lines[test_failures]=__LINE__; test_failures++; } } while (0)

// Integer sine, amplitude 1000, 20 points/cycle.
static const int16_t sine[20] = {
    0,309,588,809,951,1000,951,809,588,309,
    0,-309,-588,-809,-951,-1000,-951,-809,-588,-309
};

void __attribute__((noinline)) test_finished(void) {
    __asm__ __volatile__("nop");
}

int main(void) {
    UCSR0A = (1<<U2X0); // simulate a bootloader leaving double-speed UART enabled
    uart_init();
    CHECK(UBRR0L == 12 && UBRR0H == 0 && !(UCSR0A & (1<<U2X0)));
    CHECK(calc_apparent_power_mVA(14000,40000) == 5600);
    CHECK(calc_power_factor_x1000(2800,5600) == 500);
    CHECK(calc_power_factor_x1000(0,0) == -1);
    CHECK(calc_power_factor_x1000(5601,5600) == -1);
    capture_stats_t stats_check = {0};
    capture_stats_add(&stats_check,-3000); capture_stats_add(&stats_check,4000);
    CHECK(stats_check.minimum == -3000 && stats_check.maximum == 4000);
    CHECK(capture_stats_mean(&stats_check) == 500 && capture_stats_rms(&stats_check) == 3535);
    capture_stats_t large_stats = {0};
    for (uint16_t k=0;k<samples;k++) capture_stats_add(&large_stats,-30000000L);
    CHECK(capture_stats_rms(&large_stats)==30000000UL); // 64-bit square/accumulation
    // Independent ideal R-L source grid: 12.6/14/15.4 V, 490/500/510 Hz,
    // 2.5/5/7.5 VA, PF .6/.8/.99, and three acquisition phases.
    // Full-scale assumptions: 15.4 V, 841.8 mA peak, 7.5 W.
    for (uint8_t vi = 0; vi < 3; vi++)
    for (uint8_t si = 0; si < 3; si++)
    for (uint8_t fi = 0; fi < 3; fi++)
    for (uint8_t pi = 0; pi < 3; pi++)
    for (uint8_t phase_index = 0; phase_index < 3; phase_index++) {
        float voltage = 12.6f + vi*1.4f;
        float apparent = 2.5f + si*2.5f;
        float pf = pi == 2 ? 0.99f : 0.6f + pi*0.2f;
        float hz = 490 + fi*10;
        float irms_mA = apparent/voltage*1000;
        float ipeak = irms_mA*1.41421356f;
        float angle = acosf(pf);
        for (uint16_t k = 0; k < samples; k++) {
            float phase = k*0.000104f*hz*6.2831853f + phase_index*0.7f;
            float raw_v = 422.25f + voltage*1414.21356f/(V_MV_PER_COUNT_X100/100.0f)*sinf(phase);
            float raw_i = 300.60f + ipeak/(I_MA_PER_COUNT_X1000/1000.0f)*sinf(phase-angle);
            CHECK(raw_v > 1 && raw_v < 1022 && raw_i > 1 && raw_i < 1022);
            v_samples[k] = lroundf(raw_v); i_samples[k] = lroundf(raw_i);
        }
        float ev = fabsf(calc_voltage_rms_mV()-voltage*1000);
        float ei = fabsf(calc_current_peak_mA()-ipeak);
        float ep = fabsf(calc_power_mW()-apparent*pf*1000);
        CHECK(ev <= 770 && ei <= 42.09f && ep <= 375);
        uint32_t apparent_result = calc_apparent_power_mVA(calc_voltage_rms_mV(),calc_current_rms_mA_x100());
        CHECK(fabsf(apparent_result - (2.5f + si*2.5f)*1000) < 375);
        CHECK(abs(calc_power_factor_x1000(calc_power_mW(),apparent_result)-(int16_t)(pf*1000)) < 50);
        CHECK(labs((int32_t)calc_frequency_mHz()-(int32_t)(hz*1000)) < 200);
        CHECK(fabsf(calc_current_rms_mA_x100()/100.0f-irms_mA) <= 29.762f);
        if (ev > spec_voltage_error_mV) spec_voltage_error_mV = (uint32_t)ceilf(ev);
        if (ei*100 > spec_peak_error_mA_x100) spec_peak_error_mA_x100 = (uint32_t)ceilf(ei*100);
        if (ep > spec_power_error_mW) spec_power_error_mW = (uint32_t)ceilf(ep);
        spec_cases++;
    }
    // Non-integer-cycle windows used to create alternating power peaks even
    // with ideal sine inputs. Compare against the independently known biases.
    for (uint16_t hz = 490; hz <= 510; hz += 10) {
        for (uint8_t phase_step = 0; phase_step < 4; phase_step++) {
            float phase = phase_step * 0.6f;
            for (uint16_t k = 0; k < samples; k++) {
                float t = k * 0.000104f * hz * 6.2831853f;
                v_samples[k] = lroundf(422.25f + 177*sinf(t + phase));
                i_samples[k] = lroundf(300.60f + 146*sinf(t + phase - 0.6f));
            }
            uint32_t dc_v = adc_cycle_offset_x100(v_samples);
            uint32_t dc_i = adc_cycle_offset_x100(i_samples);
            CHECK(labs((int32_t)dc_v - 42225L) < 20);
            CHECK(labs((int32_t)dc_i - 30060L) < 20);
            float sum_v = 0, sum_i = 0;
            for (uint16_t k = 0; k < samples; k++) { sum_v += v_samples[k]; sum_i += i_samples[k]; }
            float old_error = 0, new_error = 0;
            for (uint16_t k = 0; k < samples; k++) {
                float reference = (v_samples[k]-422.25f)*(i_samples[k]-300.60f);
                float old = (v_samples[k]-sum_v/samples)*(i_samples[k]-sum_i/samples);
                float corrected = (v_samples[k]-dc_v/100.0f)*(i_samples[k]-dc_i/100.0f);
                old_error += fabsf(old-reference); new_error += fabsf(corrected-reference);
            }
            CHECK(new_error < old_error * 0.15f);
        }
    }
    // Real even-harmonic distortion is retained, not flattened to equal peaks.
    for (uint16_t k = 0; k < samples; k++) {
        float t = k * 0.000104f * 500 * 6.2831853f;
        v_samples[k] = lroundf(422 + 177*sinf(t));
        i_samples[k] = lroundf(300 + 100*sinf(t) + 20*cosf(2*t));
    }
    CHECK(labs((int32_t)adc_cycle_offset_x100(i_samples)-30000L) < 25);
    // Flat/weak inputs cannot supply cycles; use a fractional block-mean fallback.
    for (uint16_t k = 0; k < samples; k++) i_samples[k] = 360 + (k & 1);
    CHECK(adc_cycle_offset_x100(i_samples) == 36050UL);
    // Negative half-cycles must stay negative despite unsigned calibration constants.
    CHECK(scale_adc_sample(245, 42200, 10926, 100) == -19339);
    CHECK(scale_adc_sample(599, 42200, 10926, 100) == 19339);
    CHECK(scale_adc_sample(276, 42200, 3877, 1000) == -566);
    CHECK(scale_adc_sample(568, 42200, 3877, 1000) == 566);
    CHECK(scale_adc_sample(422, 42200, 10926, 100) == 0);
    CHECK(scale_adc_sample(0, 102300, 60000, 100) == -613800L);
    CHECK(scale_adc_sample(422, 42250, 10926, 100) == -55); // fractional bias retained
    long raw_capture[9] = {104, 42200, 36000, 10926, 3877, 245, 360, 599, 360};
    CHECK(validRawCapture(raw_capture, 9, 200));
    CHECK(!validRawCapture(raw_capture, 407, 200));
    raw_capture[8] = 1024; CHECK(!validRawCapture(raw_capture, 9, 200));
    long extended_wave[9] = {104, -19339, -566, 245, 276, 19339, 566, 599, 568};
    CHECK(validWaveform(extended_wave, 9, 1, 2));
    CHECK(!validWaveform(extended_wave, 13, 1, 2)); // reject overflow before reading
    CHECK(!validWaveform(extended_wave, 8, 1, 2));
    extended_wave[3] = 1024;
    CHECK(!validWaveform(extended_wave, 9, 1, 2));
    long legacy_wave[5] = {104, -19339, -566, 19339, 566};
    CHECK(validWaveform(legacy_wave, 5, 0, 2));
    // Simulate both blocks, including the final in-flight conversion.
    armed = 1;
    adc_trigger();
    for (uint16_t k = 0; k < samples; k++) {
        ADC = 422 + (int32_t)sine[k % 20] * 177 / 1000;
        ADC_vect();
        if (k == 5) {
            adc_trigger(); // unrelated extra zero crossing cannot restart an active block
            CHECK(sample_position == 6 && channel == V_CH);
        }
    }
    CHECK(sample_position == samples && flush == 1);
    ADC_vect();
    CHECK(channel == I_CH && armed == 1 && !adc_capture_done());
    CHECK((ADMUX & 0x0F) == I_CH); // real ADC1 MUX selection, not just a buffer switch
    adc_trigger();
    CHECK(sample_position == 0);  // regression: original code started I at 200
    for (uint16_t k = 0; k < samples; k++) {
        ADC = 422 + (int32_t)sine[k % 20] * 146 / 1000;
        ADC_vect();
    }
    ADC_vect();
    CHECK(adc_capture_done() && sample_position == samples);
    CHECK(i_samples[5] == 568 && i_samples[15] == 276);
    adc_status_t status;
    adc_get_status(&status);
    CHECK(status.complete && !status.capturing && !status.armed);
    CHECK(status.position == samples && status.zero_crossings == 3);
    ADC = 999;
    ADC_vect();  // an idle/stale IRQ must not alter the completed capture
    CHECK(sample_position == samples && i_samples[5] == 568);
    uint32_t voltage = calc_voltage_rms_mV();
    uint32_t current = (calc_current_rms_mA_x100() + 50) / 100;
    uint32_t power = calc_power_mW();
    CHECK(voltage > 13500 && voltage < 13800);
    CHECK(current > 390 && current < 405);
    CHECK(power > 5300 && power < 5550);

    display_update(13700, 566, 5480, 123);
    CHECK(disp_characters[DISPLAY_VOLTAGE][0] == 0x3E);  // 13.7U, suffix on DS1
    CHECK(disp_characters[DISPLAY_VOLTAGE][1] == 0x07);
    CHECK(disp_characters[DISPLAY_VOLTAGE][2] == 0xCF);
    CHECK(disp_characters[DISPLAY_VOLTAGE][3] == 0x06);
    CHECK(disp_characters[DISPLAY_CURRENT][0] == 0x77);  // 566A, integer mA peak
    CHECK(disp_characters[DISPLAY_CURRENT][1] == 0x7D);
    CHECK(disp_characters[DISPLAY_CURRENT][2] == 0x7D);
    CHECK(disp_characters[DISPLAY_CURRENT][3] == 0x6D);
    CHECK(disp_characters[DISPLAY_POWER][0] == 0x73);    // 5.48P
    CHECK(disp_characters[DISPLAY_POWER][1] == 0x7F);
    CHECK(disp_characters[DISPLAY_POWER][3] == 0xED);
    CHECK(disp_characters[DISPLAY_ENERGY][0] == 0x79);   // 0.12E, Wh
    CHECK(disp_characters[DISPLAY_ENERGY][1] == 0x5B);
    CHECK(disp_characters[DISPLAY_ENERGY][2] == 0x06);
    CHECK(disp_characters[DISPLAY_ENERGY][3] == 0xBF);
    display_set_number(DISPLAY_VOLTAGE, 1000, 2);
    CHECK(disp_characters[DISPLAY_VOLTAGE][0] == 0x3E); // preserve suffix on overflow
    CHECK(disp_characters[DISPLAY_VOLTAGE][1] == SEG_DASH);
    display_update(9995, 0, 0, 0); // rounding across 9.99 -> 10.0
    CHECK(disp_characters[DISPLAY_VOLTAGE][3] == 0x06);
    CHECK(disp_characters[DISPLAY_VOLTAGE][2] == 0xBF);
    display_update(-1, -1, -1, 10000);
    CHECK(disp_characters[DISPLAY_VOLTAGE][3] == (0x3F | SEG_DP));
    CHECK(disp_characters[DISPLAY_ENERGY][3] == 0x06); // 10.00 Wh
    CHECK(disp_characters[DISPLAY_ENERGY][2] == (0x3F | SEG_DP));
    // Mode changes at a frame boundary exactly 31250 /64 timer counts later.
    TIMER0_COMPA_vect(); // start the first voltage frame
    CHECK(display_current_quantity() == DISPLAY_VOLTAGE);
    uint32_t scan_counts = 0;
    for (uint16_t k = 0; k < DISPLAY_SCROLL_FRAMES * DISPLAY_DIGITS; k++) {
        scan_counts += (uint16_t)OCR0A + 1U;
        TIMER0_COMPA_vect();
    }
    CHECK(scan_counts == 31250UL && DISPLAY_SCROLL_SECONDS == 1);
    CHECK(display_current_quantity() == DISPLAY_CURRENT);
    for (uint16_t k = 0; k < 200; k++) TIMER0_COMPA_vect();
    CHECK(display_current_quantity() == DISPLAY_POWER);
    for (uint16_t k = 0; k < 200; k++) TIMER0_COMPA_vect();
    CHECK(display_current_quantity() == DISPLAY_VOLTAGE); // energy stays on UART

    for (uint16_t k = 0; k < 1000; k++) energy_add(5480, 31250);
    CHECK(energy_mWh() == 1522);  // 5.48 W for 1000 seconds
    energy_acc = energy_total_mWh = 0;
    for (uint8_t k = 0; k < 100; k++) energy_add(60000, 65535);
    CHECK(energy_mWh() == 3495);  // includes large increments and remainders
    energy_acc = energy_total_mWh = 0;
    energy_add(60000, 93750UL); // a 3-second telemetry interval spans a 16-bit wrap
    CHECK(energy_mWh() == 50);

    // Constant input must produce no AC current or power after offset removal.
    for (uint16_t k = 0; k < samples; k++) i_samples[k] = 422;
    CHECK(calc_current_rms_mA_x100() == 0 && calc_power_mW() == 0);
    // A half-count RMS signal must survive mean-square rounding. This used
    // to produce zero despite a varying ADC1 input. True RMS is 0.5 counts.
    for (uint16_t k = 0; k < samples; k++) i_samples[k] = 422 + (k & 1);
    CHECK(calc_current_rms_mA_x100() == (uint32_t)lroundf(0.5f*I_MA_PER_COUNT_X1000/10.0f));
    CHECK(adc_rms_counts_x100(i_samples) == 50);
    // Different channel bias must not erase AC current.
    for (uint16_t k = 0; k < samples; k++)
        i_samples[k] = 300 + (int32_t)sine[k % 20] * 146 / 1000;
    CHECK(calc_current_rms_mA_x100() > 39000 && calc_current_rms_mA_x100() < 40500);
    // Quadrature must have almost no real power; reversed polarity retains
    // the project's existing passive-load magnitude convention.
    for (uint16_t k = 0; k < samples; k++) {
        v_samples[k] = 422 + (int32_t)sine[k % 20] * 177 / 1000;
        i_samples[k] = 422 + (int32_t)sine[(k + 5) % 20] * 146 / 1000;
    }
    CHECK(calc_power_mW() < 10);
    for (uint16_t k = 0; k < samples; k++)
        i_samples[k] = 422 - (int32_t)sine[k % 20] * 146 / 1000;
    CHECK(calc_power_mW() > 5300 && calc_power_mW() < 5550);
    for (uint16_t k = 0; k < samples; k++)
        v_samples[k] = i_samples[k] = (k & 1) ? 1023 : 0;
    float full_scale_power = 511.5f*511.5f*(V_MV_PER_COUNT_X100/100.0f)
                            *(I_MA_PER_COUNT_X1000/1000.0f)/1000.0f;
    CHECK(fabsf(calc_power_mW()-full_scale_power) < full_scale_power*0.001f);
    CHECK(adc_rms_counts_x100(v_samples) == 51150); // maximum 10-bit variance

    // Compare the fixed-point path to an independent floating calculation,
    // including fractional DC bias and small signals across the ADC range.
    for (uint16_t amplitude = 1; amplitude <= 400; amplitude *= 2) {
        float total = 0, squares = 0;
        for (uint16_t k = 0; k < samples; k++) {
            uint16_t raw = 512 + (int32_t)sine[k % 20] * amplitude / 1000;
            v_samples[k] = raw;
            total += raw; squares += (float)raw * raw;
        }
        // Accumulating around a nearby offset avoids float cancellation at low amplitude.
        total = squares = 0;
        for (uint16_t k = 0; k < samples; k++) {
            float x = (int16_t)v_samples[k] - 512;
            total += x; squares += x*x;
        }
        float expected = sqrtf(squares / samples - (total / samples)*(total / samples));
        CHECK(fabsf(adc_rms_counts_x100(v_samples) / 100.0f - expected) <= 0.0051f);
    }

    // Actual 104 us cadence at the source frequency limits, not only the
    // convenient 20-sample sine above. PF=0.6 and independent channel biases.
    for (uint16_t hz = 490; hz <= 510; hz += 10) {
        for (uint16_t k = 0; k < samples; k++) {
            float phase = (float)k * 0.000104f * hz * 6.2831853f;
            v_samples[k] = 422 + (int16_t)lroundf(177.0f * sinf(phase));
            i_samples[k] = 300 + (int16_t)lroundf(146.0f * sinf(phase - 0.9272952f));
        }
        float expected_voltage = 177*0.70710678f*(V_MV_PER_COUNT_X100/100.0f);
        CHECK(fabsf(calc_voltage_rms_mV()-expected_voltage) < expected_voltage*0.02f);
        CHECK(calc_current_rms_mA_x100() > 39000 && calc_current_rms_mA_x100() < 41000);
        CHECK(calc_power_mW() > 3100 && calc_power_mW() < 3500);
    }

    long fields[4] = {0, 0, 0, 12345};
    CHECK(readNumbers(",13700,400,5480", fields, 3) == 3);
    CHECK(fields[0] == 13700 && fields[1] == 400 && fields[2] == 5480);
    CHECK(readNumbers(",1,2,3,4", fields, 3) == 4 && fields[3] == 12345);
    CHECK(readNumbers(",1,2,3oops", fields, 3) == -1);
    CHECK(readNumbers(",1,2,3,", fields, 3) == -1);
    CHECK(readNumbers(",1,2,2147483648", fields, 3) == -1);
    CHECK(readNumbers(",1,2,-2147483649", fields, 3) == -1);
    CHECK(readNumbers(",1, 2,3", fields, 3) == -1);
    CHECK(readNumbers(",1,2,3,bad", fields, 3) == -1);
    CHECK(parseDisplayMode("$D,0") == 0 && parseDisplayMode("$D,3") == 3);
    CHECK(parseDisplayMode("$D,4") == -1 && parseDisplayMode("$D,01") == -1);
    CHECK(parseDisplayMode("$D,") == -1 && parseDisplayMode("$D,1,2") == -1);
    long cal[11] = {5000,10926,3877,245,606,42800,1254,360,360,36000,0};
    CHECK(validCalibration(cal,11)); // flat current is valid telemetry
    CHECK(!validCalibration(cal,10));
    cal[9] = 36100; CHECK(!validCalibration(cal,11)); // mean outside min/max
    cal[9] = 36000; cal[2] = 0; CHECK(!validCalibration(cal,11));
    long precise_cal[13] = {5000,10926,3877,245,600,42200,1254,360,360,36000,0,12540,0};
    CHECK(validCalibration(precise_cal,13));
    precise_cal[12] = 51151; CHECK(!validCalibration(precise_cal,13));
    test_finished();
    while (1) { }
}
