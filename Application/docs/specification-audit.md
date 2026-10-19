# Specification audit

This audit separates firmware settings/calculation tests from physical bench
acceptance. No firmware was flashed and no hardware accuracy measurements were
performed during this audit. Existing calibration values, including measured
AVCC = 4.963 V, were preserved.

| Requirement | Implementation / evidence |
|---|---|
| Source 14 Vrms +/-10% | Ideal quantised-input grid includes 12.6, 14.0, 15.4 Vrms. Source voltage is an external bench setting. |
| Source 500 Hz +/-2% | Grid includes 490, 500, 510 Hz at the real 104 us sample spacing. |
| Load 2.5-7.5 VA | Grid includes 2.5, 5.0, 7.5 VA. Irms = VA/Vrms, without assuming a fixed resistance. |
| PF 0.6-0.99 | Grid includes 0.6, 0.8, 0.99 using an inductive current lag acos(PF). |
| Accuracy 5% full scale | All 243 ideal ADC cases pass the stated full-scale limits below. Physical total error requires bench verification. |
| ADC <=10 kHz | 2 MHz /16 /13 = 9615.385 conversions/s, 104 us per normal conversion. Initial dummy conversion is slower. |
| MCU clock 2 MHz | F_CPU defaults to 2000000 and other compile-time values are rejected. Physical oscillator, clock-divider fuses and actual clock still need verification. |
| Display Vrms, mA peak, W | U/A/P scroll. Current is maximum absolute sampled current after cycle-based DC removal, not RMS converted by an assumed crest factor. |
| Scroll 1 s | 50 frames, each 625 timer counts at 2 MHz /64. Test measures 31250 timer counts per mode. Independent of main-loop UART blocking. |
| UART 9600 baud | USART0 8N1, normal speed explicitly selected, UBRR0=12. Physical baud = 9615.385 (+0.1603%), normal integer-divider tolerance. ESP32 configured for 9600. |
| UART V/I/P/E | $M,Vrms_mV,Irms_mA,Power_mW,Energy_mWh,Ipeak_mA. Peak is an optional appended fifth field; old three/four-field senders remain supported. |

## Local display

The physical display remains three numeric digits plus a measurement letter:
13.7U means 13.7 Vrms; 566A means 566 mA peak; 5.48P means 5.48 W.
It cycles U -> A -> P -> U, with one second per reading. Energy is retained on
UART and the website rather than taking a fourth local scroll position.
The independent display-test build retains these fixed examples.

Timer0 alternates three 156-count digit slots and one 157-count slot. This
removes the old timer-divisor rounding error: every full frame is exactly
20 ms at the specified physical clock. ISR latency can add small edge jitter.
Fresh measurements are copied to the display before long waveform transfers.

## Calculation accuracy tests

The simulator runs the actual AVR C measurement and display code. It injects
quantised 10-bit samples from independently generated sinusoidal R-L signals:
3 voltage levels x 3 VA levels x 3 frequencies x 3 PF values x 3 start phases
= 243 cases. Each includes independent channel DC biases and checks clipping.
The ADC gains are treated as known and correct in this mathematical test.

Since a separate instrument full-scale range was not specified, test limits
use operating-range maxima: 15.4 V, 841.8 mA peak (sinusoidal 7.5 VA/12.6 V),
and a conservative 7.5 W power scale. If the assessment defines different
full-scale ranges, substitute those explicitly during bench acceptance.

| Quantity | 5% full-scale allowance | Largest simulated error |
|---|---:|---:|
| Voltage RMS | 0.770 V | 0.091 V |
| Current peak | 42.09 mA | 2.21 mA |
| Real power | 0.375 W | 0.116 W |

RMS current is also checked against 5% of 595.24 mA full scale. The required
local display uses measured peak, and UART includes both current definitions.
Energy accumulation tests cover known power/time and multi-second timer wraps.
No energy full-scale or observation interval was specified, so no separate
5%-of-full-scale energy accuracy claim is made.

These results prove calculation behaviour for ideal quantised signals, not
end-to-end hardware accuracy. Analogue gain/phase error, ADC reference/noise,
source distortion, clock error and calibration-reference uncertainty are not
measured by the simulator. V and I are captured in separate cycles, so rapidly
changing R/L still limits power accuracy until the load settles.

## Bench acceptance

1. Verify the actual MCU clock is 2 MHz, the conversion cadence is 104 us, and
   UART decodes at 9600 8N1. Verify the display changes once per second.
2. Calibrate voltage and current against an independent true-AC-RMS reference at
   the operating frequency. Verify peak current directly on a scope/current
   reference, not by multiplying RMS by sqrt(2) for a distorted waveform.
3. Repeat the grid endpoints (and nominal settings) after settling each R/L
   load. Record actual Vrms, Ipeak, real power, VA and PF simultaneously with
   a suitable reference. Check errors against the agreed full-scale allowances.
4. Pay particular attention to PF=0.6 and maximum VA: phase error matters most
   there. Gain calibration alone cannot correct analogue phase error.
5. Confirm UART contains voltage/current/power/energy and compare accumulated
   energy against independently integrated reference power over an agreed time.

The ADC and baud calculations follow the
[ATmega328PB datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/40001906C.pdf).
