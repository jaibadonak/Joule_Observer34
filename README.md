# ELECTENG 209; Team [34]; Smart Energy Monitor(Joule Observer)

Measures voltage, current and power delivered to a variable AC load, displays it locally and streams it to a phone.

Entering the **Smart Energy Challenge**: SMT board carrying its own ATmega328PB instead of an Xplained Mini, plus an app and a 3D printed enclosure.

## Key specs (Table I)

| | |
|---|---|
| Source | 14 Vrms ±10%, 500 Hz ±2% |
| Load | 2.5–7.5 VA, PF 0.6–0.99 (4 mH + 5–105 Ω) |
| Accuracy | ±5% of full scale |
| ADC | ≤10 kHz, 10-bit, 2 MHz system clock |
| Display | Vrms, RMS mA, W, Wh — 3 s scroll |
| UART | 9600 8N1, sends V, Irms, P and display mode |
| PCB | ≤20000 mm², double layer |


## Layout

```
analogue/    LTspice models, hand calcs
Software/    Single AVR project, bench checks, Proteus model
pcb/
  library/   Symbols + footprints (drawn against dispensary stock)
  altium/    Schematic, PCB, project files
  outputs/   Released gerbers + BOM (tagged releases only)
Application/ ESP32 WiFi dashboard
enclosure/   CAD + STLs
test-data/   raw/ scripts/ plots/
docs/        datasheets/ images/
DECISIONS.md Design decision log
```



Open Software/Joule_observer/Joule_observer.atsln for the AVR. See [firmware bench checks](Software/README.md) for display diagnosis and [UART/LED wiring](Application/docs/spec.md) for the ESP32.
