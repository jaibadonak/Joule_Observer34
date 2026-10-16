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
| Display | Vrms, peak mA, W — 1 s scroll |
| UART | 9600 8N1, sends V, Ipk, P and Energy |
| PCB | ≤20000 mm², double layer |


## Layout

```
analogue/    LTspice models, hand calcs
firmware/    Atmel Studio project, Proteus VSM model
pcb/
  library/   Symbols + footprints (drawn against dispensary stock)
  altium/    Schematic, PCB, project files
  outputs/   Released gerbers + BOM (tagged releases only)
app/         Phone app
enclosure/   CAD + STLs
test-data/   raw/ scripts/ plots/
docs/        datasheets/ images/
DECISIONS.md Design decision log
```


