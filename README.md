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

## Who owns what

| Member | GitHub | Subsystem |
|---|---|---|
| [Jai] | @jaibadonak | Analogue front end, power supply, PCB |
| [ ] | @ | DSP / core firmware |
| [ ] | @ | Peripherals — UART, display, ESP32, app |
| [ ] | @ | Integration, test, enclosure |

Roles are about who *builds* each block. All four of us get interviewed on all of it.

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

## Rules that matter

**Binary files don't merge.** Altium, LTspice, Proteus and CAD files get no branches and one owner. Post in Slack before you open one ("taking the PCB"), pull first, post when you've pushed. Firmware and scripts are text — branch and PR normally.

**Commit under your own account, on your UoA email.** Never push on someone else's behalf. The log is the record of who did what and it's read at every assessment.

**Decisions go in `DECISIONS.md` the day they're made.** Slack scrolls away and assessors don't read it. One entry: what, why, what we rejected.

**Tag before every assessed checkpoint** — `progress-review`, `pcb-submission`, `final-demo`.

Commit format: `area: what changed` (`pcb: add Kelvin pads to shunt`, `fw: fix ADC mux settling`).

## Dates

| | |
|---|---|
| 25–26 Aug | Progress review — analogue done in LTspice, UART on Proteus + Xplained Mini |
| **1 Sep** | **Analogue freeze.** Schematic changes stop. |
| **17 Sep** | **PCB submission.** TA check by ~8 Sep. |
| 28 Sep–2 Oct | Boards land, staged bring-up |
| 20–22 Oct | Interviews and demo |

Tests are 17 Aug and 5 Oct — 45% of the course, 40% average needed to pass.

## Notes

There is **one board**. Labs 1 and 3 give us the whole analogue circuit; the challenge changes packages to SMT and adds the MCU section. Same schematic either way.

Individual lab repos are separate. Collaborate freely, keep answers and commits your own.

Task board: Issues are grouped by `wk:NN` label.
