# Calibration and signal viewer

Calibration corrects voltage and current gain independently. It does not require
load resistance or inductance, and does not assume unity power factor. A change
of load during normal operation does not require a new calibration. Keep each
reference setting steady only while collecting its calibration captures.

## Procedure

1. Flash the latest normal AVR firmware and ESP32 esp32dev firmware. Join
   JouleObserver WiFi, open http://192.168.4.1, and select Calibration.
2. Use the intended operating frequency and let the circuit/reference instrument
   settle. Measure AVCC relative to AVR GND and enter it in volts. AVCC is the
   ADC reference in this firmware; do not drive an external voltage into AREF.
3. Measure the actual load AC RMS with a suitable reference meter or scope.
   Enter voltage in V and/or current in mA. These are the load signals before
   conditioning, not the small biased signals on the ADC pins. Leave an unused
   channel blank. Voltage-only calibration works while ADC1 is flat.
4. Wait for 8 distinct fresh captures. Progress and peak-to-peak variation are
   displayed. Selected channels must vary by no more than 1% across the window.
   Clipped and very small signals are rejected. Use a useful fraction of the
   ADC range without approaching either supply rail.
5. Hold averaged capture to freeze the collected statistics. Check the scope at
   PC0/pin 23 (V) or PC1/pin 24 (I), relative to GND, against the table. Compare
   AC RMS with DC removed; compare the DC offset separately. The table uses
   the entered AVCC to convert counts to pin voltage.
6. For one setting, download AVR settings. For a wider operating range, choose
   Record reference point, change the load/input, enter its new measured RMS,
   and collect 8 fresh captures again. Repeat at low, middle and high usable
   amplitudes. Recorded points are listed; the download uses those points.
   Clear reference points to return to a single-setting calibration.
7. Replace Software/Joule_observer/Joule_observer/calibration.h with the
   download. Rebuild and flash the normal AVR firmware, not display-test.hex.
   Calibration is not applied until this flash. The ESP32 cannot write AVR
   settings over the current UART firmware.
8. Verify using an independent amplitude/load setting and the intended frequency.
   Check both RMS readings. Check power separately with a phase-aware reference
   if power accuracy matters. A reference instrument of unknown accuracy cannot
   establish an absolute accuracy claim for this meter.

Changing AVCC alone affects ADC pin-voltage conversion, not the load gains.
Reference points are discarded after an AVR/ESP32 reset or a change in gain
metadata. Held data cannot bypass stale/disconnected telemetry checks.

## Calculation and accuracy limits

The AVR computes AC RMS after removing the capture's mean, retaining hundredths
of a count in the arithmetic and rounding to nearest. This adds arithmetic
precision, not ADC bits. The page averages the variances of 8 captures, then
square-roots the average. It does not count duplicate HTTP polls as new data.

Single-setting gain is reference RMS / raw AC RMS, in the header's fixed-point
units. Recorded points use a least-squares fit through zero after DC removal:

    gain = sum(raw_RMS * reference_RMS) / sum(raw_RMS^2)

Voltage and current are fitted independently. The largest relative discrepancy
at the entered reference points is displayed. More than 2% blocks the download
so inconsistent references are not silently combined. The 1% stability and 2%
fit thresholds are software screening choices, not guaranteed instrument accuracy.

Noise, ADC quantisation/nonlinearity, reference drift, resistor/filter response,
scope probe settings, reference-instrument uncertainty and frequency all limit
accuracy. Gain correction cannot fix clipping, a flat channel or phase error.
The fixed 200-sample block is 20.8 ms and can contain partial cycles; averaging
captures cannot remove that systematic window error. Calibrate at the intended
frequency, then verify across the loads and frequencies actually used.

The AVR captures voltage and current in separate voltage-triggered cycles.
This supports repeatable periodic loads, including inductive loads, but cannot
measure arbitrary load transients as a simultaneous two-channel instrument would.
The V*I trace and scalar power may be inaccurate if R/L changes between captures.
Energy uses the latest measured power between updates, so fast load changes are
also limited by telemetry/capture cadence.

## Signal viewer

- RMS trend: full-block AVR summaries over time; it follows changing loads.
- Calibrated waveform: all 200 original sample positions after complete-cycle DC-bias removal (block-mean fallback when cycles
  cannot be resolved) and gain scaling. Sample lines and sample points use the original values.
- Raw ADC waveform plotting is removed; the calibration table retains raw statistics.
- Sample markers, time windows, pause and keyboard/sample inspection are available.
- RMS guides and capture statistics come from the AVR for the full capture,
  independent of zoom. Raw RMS includes DC, while the calibration table's AC RMS
  excludes it. The overview's AVR RMS can refer to a newer/older capture than
  the waveform while UART packets arrive.

Calculated captures take several seconds over the 9600-baud UART, plus
diagnostics. Collecting eight distinct captures can take around a minute. Updates are therefore slower than a streaming oscilloscope.
The ADC buffers stay frozen while sending. Timer1 is extended to 32 bits so
long transfers cannot lose energy time at the old 2.1-second hardware wrap.

## Research used

Microchip AVR120 describes offset/gain calibration and the remaining ADC errors;
a gain fit cannot eliminate nonlinearity or quantisation. See
[Microchip AVR120](https://www.microchip.com/en-us/application-notes/an2559-1).
Tektronix distinguishes AC RMS from AC+DC RMS, which includes the DC offset:
[Tektronix RMS modes](https://www.tek.com/fr/support/faqs/how-do-i-use-dvm-and-frequency-counter-function-mdo3000).
These distinctions inform the separate pin-DC, pin-AC and raw-RMS displays.

Reference selection and AREF decoupling follow the
[ATmega328PB datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/40001906C.pdf).

## Alternating power peak correction

The waveform and scalar power estimate DC bias by integrating between qualified
rising crossings over whole cycles, interpolating their fractional sample
positions. This avoids confusing the remaining partial cycle with sensor bias.
It does not force equal peaks or replace samples with a fitted sine. Weak, short
or non-periodic captures fall back to their block mean. Raw ADC samples and
the RMS calibration algorithm are unchanged. Both controllers need updating for the calculated $B/$S/$G/$E format.
Interactive reference averaging and gain fitting still run in the browser;
live electrical values and waveform statistics run on the AVR.
