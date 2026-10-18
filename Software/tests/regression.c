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
#include "../../Application/src/uart_protocol.h"

volatile uint16_t test_failures = 0;
#define CHECK(condition) do { if (!(condition)) test_failures++; } while (0)

// Integer sine, amplitude 1000, 20 points/cycle.
static const int16_t sine[20] = {
    0,309,588,809,951,1000,951,809,588,309,
    0,-309,-588,-809,-951,-1000,-951,-809,-588,-309
};

void __attribute__((noinline)) test_finished(void) {
    __asm__ __volatile__("nop");
}

int main(void) {
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

    display_update(13700, 400, 5480, 123);
    CHECK(disp_characters[DISPLAY_VOLTAGE][0] == 0x3E);  // 13.7U, suffix on DS1
    CHECK(disp_characters[DISPLAY_VOLTAGE][1] == 0x07);
    CHECK(disp_characters[DISPLAY_VOLTAGE][2] == 0xCF);
    CHECK(disp_characters[DISPLAY_VOLTAGE][3] == 0x06);
    CHECK(disp_characters[DISPLAY_CURRENT][0] == 0x77);  // 0.40A
    CHECK(disp_characters[DISPLAY_CURRENT][1] == 0x3F);
    CHECK(disp_characters[DISPLAY_CURRENT][2] == 0x66);
    CHECK(disp_characters[DISPLAY_CURRENT][3] == 0xBF);
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
    // The indicator mode changes when the next frame actually begins.
    for (uint16_t k = 0; k < DISPLAY_SCROLL_FRAMES * DISPLAY_DIGITS; k++) TIMER0_COMPA_vect();
    CHECK(display_current_quantity() == DISPLAY_VOLTAGE);
    TIMER0_COMPA_vect();
    CHECK(display_current_quantity() == DISPLAY_CURRENT);

    for (uint16_t k = 0; k < 1000; k++) energy_add(5480, 31250);
    CHECK(energy_mWh() == 1522);  // 5.48 W for 1000 seconds
    energy_acc = energy_total_mWh = 0;
    for (uint8_t k = 0; k < 100; k++) energy_add(60000, 65535);
    CHECK(energy_mWh() == 3495);  // includes large increments and remainders

    // Constant input must produce no AC current or power after offset removal.
    for (uint16_t k = 0; k < samples; k++) i_samples[k] = 422;
    CHECK(calc_current_rms_mA_x100() == 0 && calc_power_mW() == 0);
    // A half-count RMS signal must survive mean-square rounding. This used
    // to produce zero despite a varying ADC1 input. True RMS is 0.5 counts.
    for (uint16_t k = 0; k < samples; k++) i_samples[k] = 422 + (k & 1);
    CHECK(calc_current_rms_mA_x100() == 193); // 1.93 mA, rather than zero
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
    CHECK(calc_power_mW() > 110000 && calc_power_mW() < 112000);

    // Actual 104 us cadence at the source frequency limits, not only the
    // convenient 20-sample sine above. PF=0.6 and independent channel biases.
    for (uint16_t hz = 490; hz <= 510; hz += 10) {
        for (uint16_t k = 0; k < samples; k++) {
            float phase = (float)k * 0.000104f * hz * 6.2831853f;
            v_samples[k] = 422 + (int16_t)lroundf(177.0f * sinf(phase));
            i_samples[k] = 300 + (int16_t)lroundf(146.0f * sinf(phase - 0.9272952f));
        }
        CHECK(calc_voltage_rms_mV() > 13500 && calc_voltage_rms_mV() < 13900);
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
    test_finished();
    while (1) { }
}
