# Final AVR project

Open `JouleObserverFinal/JouleObserverFinal.atsln` in Microchip Studio.
The target is ATmega328PB at an actual 2 MHz clock, with 9600-baud UART.

Build and check from the repository root:

```powershell
./Software-Final/build-and-test.ps1
```

Normal firmware: `JouleObserverFinal/JouleObserverFinal/Debug/JouleObserverFinal.hex`.
The separate `display-test.hex` retains the fixed 13.7U / 566A / 5.48P display test.
Its entry point is `tests/display-test.c`; normal `main.c` needs no test switches.
`int0.c` handles the zero crossing interrupt, and `uart.c` handles serial diagnostics.
The script compiles both builds, checks ADC interrupt timing and simulates
calculation tests for both current polarities. It does not flash either controller.

Calibration values remain in `calibration.h`. Complete-cycle selection, DC
offset, RMS, peak, real power and energy calculations retain their existing
precision. Waveform scaling removes DC, applies gains and calculates signed
power from unrounded counts before rounding the transmitted results.

The ESP32 website now offers calibrated sample lines or sample points, with
voltage/current overlay and RMS history. Raw ADC telemetry is retained for
calibration, but raw ADC plotting and smooth interpolation are removed.
