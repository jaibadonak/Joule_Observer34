# AVR firmware and bench checks

Open `Joule_observer/Joule_observer.atsln` in Microchip/Atmel Studio. This is the
only AVR project, targeting **ATmega328PB at an actual 2 MHz CPU clock**. It is
based on the newer former `Testing/Project_testing` implementation, using a
separate measured DC offset for each channel and RMS current. The old duplicate
and the placeholder UART project have been removed.

Normal firmware now also sends the AVR energy total in $M, a full 200-pair $Y
raw waveform capture, and full-block $C calibration statistics once per telemetry
interval. Flash the updated ESP32 firmware for the new dashboard. Calibration
constants are in Joule_observer/Joule_observer/calibration.h; use the dashboard's
Calibration panel to compare ADC pin measurements and download a replacement.
Rebuild and flash the AVR to apply it. The display test is still independent
and sends fixed summaries without simulated live waveform/calibration packets.

See ../Application/README.md for the scope and calibration procedure. Capture
timeouts start after rearming, so the new UART telemetry time is not incorrectly
charged to the following capture.

## Findings from 18 October 2026

- The older AVR project's ADC never reset `sample_position` between voltage and
  current. The first current sample went to `i_samples[200]`, past the end of the
  200-element array, corrupting RAM instead of filling the current block. This
  explains a possible combination of zero current/power and corrupted display
  buffers. The newer project already had the index reset; its zero-current
  symptom therefore still needs the raw input checks below.
- The older project used ADC /128: 832 us per sample at 2 MHz. The retained
  project uses /16: 104 us, approximately 9.6 kHz, below the 10 kHz limit.
- The newer project's energy constant was 56,250,000 instead of 112,500,000
  mW-ticks per mWh. That doubled energy; it did not explain zero energy. Fixed.
- Capture restarts now abort old conversions and reset state atomically, so a
  timeout cannot race an ADC interrupt. Display buffer copies preserve the
  caller's interrupt state. Scheduler counters no longer overflow at 16 bits.
- Energy accumulation avoids overflow when adding a large increment to a saved
  remainder. Power scaling also avoids signed multiplication overflow.
- `$D,0` through `$D,3` report the real display scan mode to the website.
  ESP32 indicator outputs have been removed. The AVR shows a U/A/P/E suffix
  on its right-most digit. The one-second scroll shows Vrms, mA peak and W;
  energy remains on UART/the website.
- The display scan now allows ADC/INT0 to interrupt its bit-banging, while
  masking its own interrupt source to prevent reentry. Otherwise the display
  ISR can block more than one 104 us conversion and drop ADC samples.
- ADC results are read before index/pointer processing, idle interrupts are
  ignored, and every buffer write is bounded. Stopping free-run uses an
  explicit register write that preserves ADIF rather than accidentally
  clearing a pending completion with read/modify/write.
- Mode reporting switches at the first digit of the new frame. ESP32 parsing
  rejects overflowing numbers, malformed optional fields, and incomplete
  waveform pairs without replacing good readings.
- All firmware timing shares config.h's 2 MHz definition. An incompatible
  F_CPU fails compilation. Both Studio Debug and Release use -Os so the ADC
  interrupt timing matches the verified final firmware.

The local display and `$M` now use the same saved voltage/current/power values.
No voltage gain was changed to force a 13.7 V result. The connector drawing
does not show segment wiring or LED driver circuitry, so it cannot establish
segment order, digit polarity, or decimal-point wiring.

## Flash and distinguish measurement faults from display faults

1. Build and flash this AVR project and the ESP32 **esp32dev** environment.
   `mock` generates its own voltage, ignores UART, and cannot verify AVR readings.
   AVR TX PD1 goes through the level shifter to ESP32 GPIO16. The user confirmed
   the attached schematic's RXD0/GPIO3 label was wrong. Keep common ground.
2. For a display-only test, add `DISPLAY_SELF_TEST` to AVR compiler defined
   symbols, rebuild, and flash. It scrolls **13.7U**, **566A** (mA peak), **5.48P**
   every one second and sends `$M,13700,400,5480,123,566` plus mode messages.
   All calculations and sensor inputs are bypassed in this build.
3. If those values look wrong locally but correct over UART, inspect DS1–DS4
   order (bench confirmed DS1 right, DS4 left; enables active-low), decimal point
   (QH/bit 7), segments (QA–QG = a–g, active-high), and CLR. CLR must be held
   inactive/high at the display board; it is not driven by the AVR schematic.
   The fixed test checks the existing assumptions without guessing new ones.
4. Remove `DISPLAY_SELF_TEST`, rebuild, and flash for real measurements. The
   ESP32 USB log echoes `$M`, `$D`, and once-per-second `V raw` / `I raw` lines
   containing min/max/average ADC counts. They are taken from frozen buffers.
   Voltage around 13.7 Vrms should have roughly 177 counts peak swing using the
   current gain constants. A 400 mA RMS sine should swing about 146 counts peak.
5. If `I raw` min and max are equal, ADC1 has no measured AC swing: inspect Vif
   at **PC1, physical pin 24**, its RC filter, shunt amplifier, and return. If
   the raw values vary but are clipped at 0 or 1023, inspect analogue levels and
   AVCC/AREF. PC0/pin 23 is voltage. These are physical package pin numbers,
   not Arduino GPIO numbers. A correctly centred constant current input yields
   zero current/power after DC-offset removal, and energy then stays zero.

At 5 W the internal energy total first advances by 1 mWh after about 0.72 s;
the three-digit local display rounds to 0.01 Wh after about 3.6 s. Full mWh
precision remains available on the website/UART. Energy starts over after
resetting the AVR. If no complete capture
arrives for 0.5 s, readings become zero; check the voltage zero-crossing signal
at PD2/INT0. Capturing separate V/I blocks assumes a stable periodic source.

See `../Application/docs/spec.md` for UART wiring, local suffixes and telemetry.

## Command-line verification

Run `./build-and-test.ps1` from this directory, or
`./Software/build-and-test.ps1` from the repository root. It builds the normal
and fixed-display firmware with warnings treated as errors, writes HEX files
under `Joule_observer/Joule_observer/Debug/`, and runs `tests/regression.c` in
the AVR instruction simulator. Final normal firmware is `Joule_observer.hex`;
the independent display test is `display-test.hex`. The script also checks the
compiled ADC ISR's longest instruction path: 117 of 208 available CPU cycles
(58.5 of 104 us), including vector entry. That bound excludes delays caused by
other interrupts; the scan allows preemption specifically to limit those.
The build script defaults to PlatformIO's Python for that instruction check;
override its Python, Toolchain or DevicePack parameters for other installations.
Regression coverage includes quadrature/reversed/clipped waveforms, constant
current input, energy increments, display formatting and mode boundaries, and
the same UART parser used by ESP32. Hardware timing, analogue calibration, wiring,
and segment polarity require the bench checks above.

## Follow-up: voltage correct but current/power/energy zero

The live firmware computes voltage only after BOTH voltage and current blocks
complete. A capture that never starts, or stalls between V and I, times out after
0.5 s and clears voltage as well. Thus a stable, updating live voltage reading
is evidence of completed paired captures, rather than a completely missing
zero-crossing input. Current sampling uses ADC1/PC1 and the same voltage INT0
edge as voltage sampling. The current zero-crossing input on PD3 is unused.

The ADC channel switch, buffer indexing, free-run stop/drain, ADC interrupt
budget and independent offset/scaling calculations were checked again. Tests
now assert that ADMUX actually selects ADC1, extra edges cannot restart an
active block, and a completed snapshot reports both blocks finished. Synthetic
waveforms sampled at 104 us pass at 490/500/510 Hz, PF 0.6 and different DC
biases on V and I. The simulator supplies ADC results explicitly; it cannot
establish the actual voltages on the board or emulate ADC analogue settling.

A genuine small-signal calculation fault was fixed: rounding mean-square ADC
counts to an integer before the square root suppressed signals below one count
RMS. Fractional mean-square and DC-offset precision are now retained. Alternating
ADC1 readings of 422 and 423 give 1.93 mA instead of zero. This is too small a
fault to explain zero at a normal several-hundred-milliamp load. ADC0/ADC1 are
also explicitly configured as inputs with pull-ups off at initialization.

UART now identifies the build with `BOOT ... LIVE` or `DISPLAY_SELF_TEST`.
`V raw` and `I raw` include min/max/average/span, followed by
`I rms_mA_x100` (100 = 1 mA). Incomplete captures print:

```
ADC timeout: ch=0 pos=0 armed=1 active=0 zc=123 mux=64 ctrl=...
```

- `ch=0` is V; `ch=1` is I; `pos` is stored samples (0–200).
- `armed=1` means waiting for the next voltage zero crossing.
- `zc` counts voltage interrupt edges modulo 65536; compare successive logs.
- `active=1` means converting or draining the final in-flight conversion.
- `mux` and `ctrl` are decimal snapshots of ADMUX and ADCSRA before restart.
- Completed buffers with `I raw` span 0 contain no measured AC variation;
  current/power/energy then correctly stay zero. A 400 mA RMS sine should have
  approximately 292 counts peak-to-peak using the present calibration.

On the connected PC, the ESP32 briefly enumerated as CP210x COM11, but opening
it returned Windows error 31 (device not functioning), then it disappeared.
No live readings were captured. Once its USB connection is stable, read COM11
(or its newly assigned port) at 115200 baud; no ESP32 update is needed to echo
the new AVR diagnostic lines.

Calibration and full-capture viewer changes: see ../Application/docs/calibration.md.
RMS arithmetic now retains rounded hundredths of ADC counts. Timer1 is extended
to 32 bits so full 9600-baud captures cannot lose elapsed energy time.

Latest specification audit: ../Application/docs/specification-audit.md.
