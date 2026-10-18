# UART and dashboard API

ATmega328PB (2 MHz) -> USART0 TX/PD1 -> level shifter -> ESP32 GPIO16/UART2 RX.
ESP32 GPIO17 TX is reserved for AVR PD0/RX; the AVR currently only transmits.
9600 baud, 8N1, ASCII lines, LF or CRLF. Common ground is required.
USB diagnostics are echoed at 115200 baud. ESP32 pins are not 5 V tolerant.

## Packets

Every numeric field is a decimal integer. Empty fields, overflow, trailing
commas, non-numeric suffixes and invalid packet shapes are rejected. Unknown
human-readable diagnostics are ignored. Malformed $ packets are counted.
Lines longer than 1023 bytes are dropped as a whole.

### Measurements

    $M,<vrms_mV>,<irms_mA>,<power_mW>[,<energy_mWh>]

RMS voltage/current must be non-negative. Power may be signed. Optional energy
must be non-negative. Older three-field packets keep working, with energy null.
Additional numeric fields are validated and ignored. The new AVR sends energy
from its own accumulator; the browser does not estimate missing energy.

### Local display mode

    $D,<mode>

Exactly one digit: 0 voltage, 1 current, 2 power, 3 energy. The AVR repeats mode
every 250 ms and on changes. The ESP32 uses this only for website metadata,
showing mode unavailable after 1.5 s without a packet. It does not configure
or drive any indicator GPIOs; GPIO25/26/27/32 are no longer assigned.

The AVR's right-most digit (DS1) shows U/A/P/E for voltage/current/power/energy.
The three numeric digits show V/A/W/Wh, respectively: 13.7U, 0.40A, 5.48P,
0.12E. Precision adjusts to fit three digits, up to two decimals. U is the
seven-segment voltage symbol. UART and website current remain in mA.
UART is GPIO16, as confirmed by the user; the schematic's GPIO3 label was wrong.

### Waveform

    $W,<dt_us>,<v0_mV>,<i0_mA>,<v1_mV>,<i1_mA>,...

2–64 complete pairs are retained; extra pairs are validated then ignored. dt_us
must be 1–1000000. Values are signed 32-bit integers (no silent int16 clipping).

The AVR sends the first 32 contiguous samples from each frozen 200-sample block,
104 us apart, approximately once per second. Each block starts at a rising
voltage zero crossing, but V and I are captured in separate cycles. A V×I
waveform therefore assumes a stable periodic load. It is derived data, not a
simultaneous two-channel acquisition. The scalar real power uses all 200 pairs.
The display-test firmware sends fixed summaries without fabricated waveforms.

The ADC buffers remain frozen while the line is transmitted. Capture timeouts
are measured from the actual rearm time, excluding preceding UART transmission.
At typical lab values, waveform + raw diagnostics + calibration + summary/mode
packets fit the 960-character/s UART budget.

### Calibration

    $C,<reference_mV>,<v_scale_x100>,<i_scale_x1000>,<v_min>,<v_max>,<v_mean_x100>,<v_rms_x10>,<i_min>,<i_max>,<i_mean_x100>,<i_rms_x10>

Exactly 11 fields. Reference is 1000–5500 mV; gains are 1–60000. Raw min/max
must be 0–1023 with min <= max. Mean is in hundredths of an ADC count and lies
between min/max. RMS is AC-only, in tenths of a count, after DC removal.
These statistics cover the complete 200-sample blocks, not only the 32-point
waveform snapshot.

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
| energy_mWh | AVR total, or null for old three-field packets |
| display_mode | 0–3, or -1 when stale/unavailable |
| wave_sequence, wave_age_ms | Valid W count and age, or null age before one arrives |
| dt_us, v_mV, i_mA | Latest waveform; empty arrays until a valid W packet |
| invalid_packets, dropped_lines | Parser and oversized-line counters |
| calibration | null, or object below |

Calibration object fields: age_ms, adc_reference_mV, voltage_scale_x100,
current_scale_x1000, v_min, v_max, v_mean_x100, v_rms_x10, i_min, i_max,
i_mean_x100, i_rms_x10.

The page polls every 500 ms with a 2 s fetch timeout. Measurement and waveform
freshness are independent (3 s threshold). A failed HTTP response never switches
to demo data. Session history is browser-local, bounded to 300 distinct fresh
measurement packets. Reset markers, falling energy or ESP32 uptime clear it.
Apparent power = Vrms*Irms. PF is unavailable at zero apparent power or for a
ratio outside [-1,1]. Frequency is estimated from waveform rising crossings,
not hard-coded. Energy is a history chart; it has no AC waveform.
