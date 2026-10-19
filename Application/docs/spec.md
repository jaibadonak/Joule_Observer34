# UART and dashboard API

ATmega328PB (2 MHz) -> USART0 TX/PD1 -> level shifter -> ESP32 GPIO16/UART2 RX.
ESP32 GPIO17 TX is reserved for AVR PD0/RX; the AVR currently only transmits.
9600 baud, 8N1, ASCII lines, LF or CRLF. Common ground is required.
USB diagnostics are echoed at 115200 baud. ESP32 pins are not 5 V tolerant.

## Packets

Every numeric field is a decimal integer. Empty fields, overflow, trailing
commas, non-numeric suffixes and invalid packet shapes are rejected. Unknown
human-readable diagnostics are ignored. Malformed $ packets are counted.
Lines longer than 2047 bytes are dropped as a whole.

### Measurements

    $M,<vrms_mV>,<irms_mA>,<power_mW>[,<energy_mWh>[,<ipeak_mA>]]

RMS voltage/current must be non-negative. Power may be signed. Optional energy
must be non-negative. Older three-field packets keep working, with energy null.
An optional fifth field is non-negative sampled peak current in mA. Three-
and four-field legacy packets remain accepted; absent peak current is null.
Further numeric fields are validated and ignored. The new AVR sends energy
from its own accumulator; the browser does not estimate missing energy.

### Local display mode

    $D,<mode>

Exactly one digit: 0 voltage RMS, 1 peak current, 2 power; 3 energy is accepted
for legacy senders but no longer included in the AVR local scroll. The AVR repeats mode
every 250 ms and on changes. The ESP32 uses this only for website metadata,
showing mode unavailable after 4 s without a packet. It does not configure
or drive any indicator GPIOs; GPIO25/26/27/32 are no longer assigned.

The AVR scrolls Vrms -> peak current in mA -> power in W, one second each.
Its right-most digit identifies U/A/P; A is a current-mode marker and the
number is milliamps peak, not RMS amps. Examples: 13.7U, 566A, 5.48P.
Three numeric digits cover the specified range. Energy remains on UART and
the website; its display formatting buffer is retained but is not scrolled.
The scan uses 625 /64 Timer0 counts per frame, 50 frames per second, with
one extra timer count in one of the four digit slots to remove truncation.
Current RMS remains the M packet's second field and powers the website's
RMS history/PF calculation. Peak current is appended as the fifth field.
UART is GPIO16, as confirmed by the user; the schematic's GPIO3 label was wrong.

### Waveform

    $W,<dt_us>,<v0_mV>,<i0_mA>,<v1_mV>,<i1_mA>,...

Legacy $W and the previous extended $X packets remain supported:

    $X,<dt_us>,<v0_mV>,<i0_mA>,<v0_adc>,<i0_adc>,<v1_mV>,<i1_mA>,<v1_adc>,<i1_adc>,...

Each group contains the calibrated signed V/I samples and their original
unsigned 10-bit ADC readings (0–1023), including the DC bias. Both views refer
to exactly the same sample positions. Packets require 2–200 complete groups within the line-size limit;
extra groups are rejected before reading the sample buffer. dt_us
must be 1–1000000. Values are signed 32-bit integers (no silent int16 clipping).

The new AVR sends the complete 200-sample block in a compact raw packet:

    $Y,<dt_us>,<v_dc_x100>,<i_dc_x100>,<v_gain_x100>,<i_gain_x1000>,<v0_adc>,<i0_adc>,...

Each DC bias is in hundredths of an ADC count. For stable periodic signals,
it is integrated across complete cycles with fractional crossing boundaries.
If fewer than two qualified crossings are present, or the signal is weak/noisy,
the estimate falls back to the block mean. Field positions and units are
unchanged from the earlier Y packet. Gains and DC estimates travel with the
capture, so calibrated and raw traces always use the same capture settings.
The ESP32 validates all fields and reconstructs signed scaled values. W and X
remain accepted for older firmware. Up to 200 sample pairs are supported.

Samples are 104 us apart (20.7 ms first-to-last span). V and I are captured in
separate rising-voltage-triggered cycles; their product assumes a repeating
load. R/L changes between captures invalidate that assumption. The scalar
power uses all 200 pairs. Display-test sends summaries without waveforms.

A complete Y line is below the 2047-byte line limit even at 1023 ADC counts.
It occupies approximately 1.7-2.1 seconds at 9600 baud, plus diagnostics.
Buffers stay frozen until transmission ends. Timer1 is extended to 32 bits to
retain elapsed energy time through long transfers. Capture timeouts still
start at rearm. Updates are not a continuous oscilloscope stream.

### Calibration

    $C,<reference_mV>,<v_scale_x100>,<i_scale_x1000>,<v_min>,<v_max>,<v_mean_x100>,<v_rms_x10>,<i_min>,<i_max>,<i_mean_x100>,<i_rms_x10>

11 legacy fields, or 13 fields with v_rms_x100 and i_rms_x100 appended. Reference is 1000–5500 mV; gains are 1–60000. Raw min/max
must be 0–1023 with min <= max. Mean is in hundredths of an ADC count and lies
between min/max. RMS is AC-only, in tenths of a count, after DC removal.
These statistics cover the complete 200-sample blocks. The new RMS fields
are rounded to nearest hundredth of an ADC count (0-51150); old tenths fields
remain for compatibility. Each API calibration object also has a sequence
number for distinct-capture averaging.

ADC input mV/count = reference_mV / 1024.
Load mV/count = v_scale_x100 / 100.
Load mA/count = i_scale_x1000 / 1000.
Power scale (mW/count² x 10000) = round(v_scale_x100*i_scale_x1000/10000).

The page uses raw counts to compare ADC pin signals to a scope and known LOAD
RMS values to calculate gain constants. One bad/flat channel does not prevent
calibrating the other with its reference left blank. Clip checks apply to the
channel being adjusted. Calibration is a browser workflow generating
calibration.h; it does not send settings back to the AVR.

## HTTP

The ESP32 AP is JouleObserver (open, 192.168.4.1). DNS and unknown paths redirect
to /. The root serves a gzip-compressed embedded HTML page with no external
assets. /api/data returns JSON with no-store caching.

| Field | Meaning |
|---|---|
| source | uart or mock (browser file/?demo uses demo locally) |
| age_ms | ms since last valid M packet, or null before the first |
| sequence | Count of valid M packets, used to avoid duplicate history points |
| epoch | Increments on an AVR BOOT marker |
| uptime_ms | ESP32 uptime; a decrease also resets browser session history |
| vrms_mV, irms_mA, power_mW | Last valid measurement |
| ipeak_mA | Sampled absolute AC peak current in mA, or null for legacy packets |
| energy_mWh | AVR total, or null for old three-field packets |
| display_mode | 0–3, or -1 when stale/unavailable |
| wave_sequence, wave_age_ms | Valid W/X/Y count and age, or null age before one arrives |
| dt_us, v_mV, i_mA | Latest waveform; empty arrays until a valid W/X/Y packet |
| v_adc, i_adc | Original ADC counts from X/Y; empty for legacy W or mock |
| invalid_packets, dropped_lines | Parser and oversized-line counters |
| calibration | null, or object below |

Calibration object fields: age_ms, adc_reference_mV, voltage_scale_x100,
current_scale_x1000, v_min, v_max, v_mean_x100, v_rms_x10, i_min, i_max,
i_mean_x100, i_rms_x10, and optional v_rms_x100 / i_rms_x100; sequence is the
count of valid C packets.

The page polls every 500 ms with a 2 s fetch timeout. Measurement and waveform
freshness are independent (3 s threshold). A failed HTTP response never switches
to demo data. Session history is browser-local, bounded to 300 distinct fresh
measurement packets. Reset markers, falling energy or ESP32 uptime clear it.
Apparent power = Vrms*Irms. PF is unavailable at zero apparent power or for a
ratio outside [-1,1]. Frequency is estimated from waveform rising crossings,
not hard-coded. Energy is a history chart; it has no AC waveform.

The scope selector offers calibrated signals or raw ADC counts for voltage and
current. Raw view retains DC bias and uses a 0–1023 axis. Its RMS includes DC.
Calibrated Window RMS uses only the visible samples; overview RMS uses the
complete 200-sample AVR block. They can differ for a partial-cycle window.

Calibration now averages eight fresh captures, screens selected-channel stability,
and optionally fits multiple independent reference points. See [calibration guide](calibration.md).

Power and calibrated waveforms use complete-cycle DC bias estimates, avoiding
the alternating peak artifact caused by subtracting a partial-cycle block mean.
RMS/calibration statistics still describe their full 200-sample window; raw
ADC codes are never altered. Cycle detection requires at least 8 samples per
period and rejects more than 20% period spread.

See [specification audit](specification-audit.md) for the 243-case calculation
checks, exact scroll timing and required bench acceptance measurements.
