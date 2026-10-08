# Joule Observer data formats

How the readings get from the ATmega328P to a phone:

```
ATmega328P ──UART, 9600 8N1──▶ ESP32 UART2 (GPIO16) ──WiFi "JouleObserver"──▶ phone
             $M and $W lines                         captive portal, GET /api/data (JSON)
```

Every value is a whole number in milli-units (mV, mA, mW) or microseconds, all the way from the ATmega to the page. Only the page divides by 1000, for display.

The ESP32 side is `src/main.cpp` and the page is `web/index.html`. If you change a format here, change both of those too.

## 1. ATmega → ESP32: UART lines

| Setting | Value |
|---|---|
| Serial | 9600 baud, 8 data bits, no parity, 1 stop bit (the course UART spec) |
| Wiring | ATmega TXD (PD1) → level shifter → ESP32 GPIO16 (UART2 RX). ESP32 GPIO17 (UART2 TX) → level shifter → ATmega RXD (PD0), wired but unused. Common ground. |
| Text | ASCII, one message per line, ending in `\n` or `\r\n` |

ESP32 pins are 3.3 V and not 5 V tolerant, so keep the level shifter. GPIO16 and 17 are free on the DevKitC-32E's WROOM-32E module. WROVER modules use them for PSRAM.

Rules:

- A message is `$`, a letter, then numbers, each after a comma: `$M,14020,431,5620`. Numbers are whole and decimal, optionally negative, with no spaces or decimal points.
- Lines that aren't `$M` or `$W` messages are ignored. The ATmega can keep printing human-readable text for a terminal on the same UART.
- A garbled message (too few numbers, or a field that isn't a number) is dropped whole. The dashboard keeps the last good one.
- Extra numbers at the end of a `$M` line are ignored, so fields such as energy can be added later without breaking the ESP32.
- Lines longer than 1023 characters are dropped.

### `$M`: readings

`$M,<vrms_mV>,<irms_mA>,<power_mW>`

| Field | Unit | Example |
|---|---|---|
| vrms_mV | mV | 14020 = 14.02 V |
| irms_mA | mA | 431 |
| power_mW | mW, real (average) power, negative if the current is reversed | 5620 = 5.62 W |

The dashboard greys out readings more than 3 s old, so send one at least that often. Once a second, with the LCD update, is the natural rate.

### `$W`: waveform snapshot

`$W,<dt_us>,<v0>,<i0>,<v1>,<i1>,...`

| Field | Unit |
|---|---|
| dt_us | µs between consecutive samples of the same channel |
| v*k*, i*k* | mV and mA, in pairs. 2 to 64 pairs (any after the 64th are ignored), each value −32768 to 32767 |

- Start the snapshot at a rising zero crossing of the voltage (the zero-crossing detector gives you this). That way the trace stays still from one update to the next.
- The page treats v*k* and i*k* as taken at the same moment. If the ADC alternates between channels, each current sample is really one conversion later than its voltage sample: 100 µs at 10 kHz, or 18° at 500 Hz. Correct for that in firmware if the phase on screen needs to be exact.
- Suggested: 20 pairs at 200 µs, which is two 500 Hz cycles. 200 µs is the per-channel interval when a 10 kHz ADC alternates between V and I. More pairs give a smoother trace.

### Example

14.02 V, 431 mA and 5.62 W (power factor 0.93), two cycles:

```
$M,14020,431,5620
$W,200,0,-224,11654,152,18857,470,18857,608,11654,514,0,224,-11654,-152,-18857,-470,-18857,-608,-11654,-514,0,-224,11654,152,18857,470,18857,608,11654,514,0,224,-11654,-152,-18857,-470,-18857,-608,-11654,-514
```

### UART budget

9600 baud carries about 960 characters a second. The two lines above are 229 characters (0.24 s), so one `$M` and one `$W` a second leaves room for human-readable text. Two of each a second is about the limit.

### Sending from the ATmega

This uses `USART_transmit()` and `USART_string()` from `firmware/UART_209/UART_209/main.c`. Each character waits for the UART, so a `$W` line keeps the main loop busy for about 0.2 s. Take samples in the ADC interrupt so that doesn't matter.

```c
// Prints a whole number, e.g. -1234, using 16-bit maths (fast at 2 MHz).
void USART_int(int16_t x)
{
	char digits[5];
	uint8_t n = 0;
	uint16_t u = x;

	if (x < 0)
	{
		USART_transmit('-');
		u = -u;
	}
	do
	{
		digits[n++] = '0' + u % 10;
		u /= 10;
	} while (u);
	while (n)
	{
		USART_transmit(digits[--n]);
	}
}

void send_readings(int16_t vrms_mV, int16_t irms_mA, int16_t power_mW)
{
	USART_string("$M,");
	USART_int(vrms_mV);
	USART_transmit(',');
	USART_int(irms_mA);
	USART_transmit(',');
	USART_int(power_mW);
	USART_string("\r\n");
}

void send_waveform(int16_t dt_us, const int16_t *v_mV, const int16_t *i_mA, uint8_t pairs)
{
	USART_string("$W,");
	USART_int(dt_us);
	for (uint8_t k = 0; k < pairs; k++)
	{
		USART_transmit(',');
		USART_int(v_mV[k]);
		USART_transmit(',');
		USART_int(i_mA[k]);
	}
	USART_string("\r\n");
}
```

## 2. ESP32 → phone: WiFi

| Setting | Value |
|---|---|
| Network | `JouleObserver`, open (no password). Set `WIFI_NAME` and `WIFI_PASSWORD` in `src/main.cpp` |
| ESP32 address | 192.168.4.1 |
| DNS | Every name the phone looks up resolves to 192.168.4.1 |

### Captive portal

When a phone joins a network, it fetches a known "am I online?" URL: `/generate_204` on Android, `/hotspot-detect.html` on iPhone, `/connecttest.txt` on Windows, and so on. The ESP32 points every name at itself and redirects every URL it doesn't serve to `http://192.168.4.1/`. The phone takes the redirect to mean it's on a sign-in network and opens its sign-in screen, which shows the dashboard.

| Request | Response |
|---|---|
| `GET /` | The dashboard (`web/index.html`, built into the firmware) |
| `GET /api/data` | Latest readings as JSON, below |
| Anything else | 302 redirect to `http://192.168.4.1/` |

### `GET /api/data`

The page fetches this every 250 ms.

```json
{"source":"uart","age_ms":250,"vrms_mV":14020,"irms_mA":431,"power_mW":5620,
 "dt_us":200,"v_mV":[0,11654,18857,18857,11654,0,...],"i_mA":[-224,152,470,608,514,224,...]}
```

| Field | Meaning |
|---|---|
| source | `"uart"`: real data from the ATmega. `"mock"`: the `mock` firmware build, made-up data. |
| age_ms | ms since the last `$M` line, or `null` if none has arrived since the ESP32 started |
| vrms_mV, irms_mA, power_mW | From the last `$M` line |
| dt_us, v_mV, i_mA | From the last `$W` line. The arrays are empty until one arrives. |

Opened straight from a file, the page makes up data in this same shape with `"source":"demo"`.
