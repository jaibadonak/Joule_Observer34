# Joule Observer dashboard

The ESP32 hosts this page on the JouleObserver WiFi network at http://192.168.4.1.
The page is offline-capable: no external fonts, scripts, chart libraries or CDN.

## Features

- RMS voltage/current, real power, and AVR energy total.
- History for every reading, session min/max, apparent power and power factor.
- Actual voltage/current captures with sample lines or sample points,
  V/I overlay, AVR-calculated instantaneous power,
  time-window controls, pause, keyboard/sample inspection and CSV history export.
- Separate measurement and waveform freshness, UART error counters, display mode.
- Display mode is metadata only; there are no ESP32 indicator GPIO outputs.
  The AVR scrolls Vrms / mA peak / W every one second, labelled U/A/P.
  Energy remains on the website and UART; RMS current is retained alongside peak.
- Calibration mode with raw ADC min/max/DC/AC RMS/peak-to-peak, capture hold,
  reference inputs and a downloadable AVR calibration.h file.
- Clear waiting, disconnected, stale, demo and mock states. Missing values are
  shown as unavailable; missing waveforms are never synthesised for live data.

## Preview and test

Open web/index.html directly for a clearly labelled demo, or serve web/ using
a local HTTP server and open /?demo. Demo and mock calibration downloads are disabled.

Run the dashboard tests with Node:

    node Application/tests/dashboard.test.cjs

Run from the repository root. The tests cover waveform math, current-zero and
missing-data cases, packet deduplication, AVR reset handling, calibration bounds,
stale calibration captures and CSV formatting.

## Build and flash

    pio run -e esp32dev
    pio run -e esp32dev -t upload --upload-port COM11

Run in Application/. Replace COM11 with the actual CP210x port and close any
serial monitor first. The mock environment generates simulated packets and
ignores the AVR UART. Use esp32dev for real hardware.

The build script compresses web/index.html into web/index.html.gz and embeds
the gzip asset. It is served with Content-Encoding: gzip; no filesystem upload
is needed. Rebuild/upload after changing the page.

Build and flash the AVR too for waveform, energy and calibration telemetry:

    ./Software-Final/build-and-test.ps1

Normal AVR output:
Software-Final/JouleObserverFinal/JouleObserverFinal/Debug/JouleObserverFinal.hex

The independent display-test.hex build remains available. Existing AVR firmware
sending three-field $M packets still works; energy, waveforms and calibration
are unavailable until the AVR is updated.

## Calibration

See [the calibration guide](docs/calibration.md) for setup, voltage-only
calibration, averaged captures, multiple reference points and accuracy limits.
The page also includes the steps beside its fields. Calibrate voltage/current
independently using measured AC RMS, with R/L held steady for each capture.
Normal readings support changing loads without entering resistance/inductance.


## Reference

The related [CompSys209 project](https://jmw.nz/projects/CompSys209) informed
the emphasis on actual cycle-aligned waveforms, RMS rather than peak-only
estimates, CSV logging and gzip delivery. Our ADC uses an interrupt-driven
capture and a preemptible display scan; its timing is checked separately.
See docs/spec.md for exact packet formats and limitations.

## Waveform viewer

Select Voltage, Current or Power, then Waveform. Choose sample lines or sample
points, adjust the time window, or overlay voltage/current. Smooth interpolation
and raw ADC plotting are removed. Capture statistics cover the full capture,
independent of zoom. Raw calibration telemetry remains in the Calibration panel.
Voltage frequency, last-reading age and local-display mode are no longer shown
on the dashboard. Freshness checking still identifies stale measurements.

Specification verification: see [the audit](docs/specification-audit.md).
