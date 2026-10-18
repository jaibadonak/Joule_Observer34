# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ELECTENG 209 (University of Auckland) Team 34 project, "Joule Observer": a smart energy monitor that measures voltage, current and power delivered to a variable AC load (14 Vrms, 500 Hz, 2.5–7.5 VA, PF 0.6–0.99), shows them on a local display and sends them over UART (9600 8N1: V, Ipk, P, Energy) to a phone. The team is entering the Smart Energy Challenge, which means one double-layer SMT PCB with its own ATmega328PB at a 2 MHz system clock (no Xplained Mini), plus a phone app and a 3D-printed enclosure. See the README for the full spec table, owners and dates.

Most of the repo is binary or tool-native design files (LTspice, Proteus, Altium, CAD). The hand-written source is the ATmega firmware (`firmware/`) and the ESP32 WiFi dashboard (`Application/`).

## Repo map

- Software/Joule_observer/Joule_observer.atsln is the only AVR solution, targeting ATmega328PB. It contains ADC capture, per-channel DC-offset removal, RMS voltage/current, real power, energy, multiplexed display, and USART0. The obsolete placeholder UART project and duplicate Testing project were removed on 18 October 2026.
- `firmware/UART_SIM.pdsprj` is the Proteus VSM simulation of the firmware.
- `analogue/MASTER_SCHEMATIC_EE209.asc` and `LTspice-Final/` hold the LTspice models of the analogue front end: the signal conditioning (`sig_cond*`, first/second order, voltage and shunt-current channels), the zero-crossing detector, and `FinalParentSchematic.asc`, which instantiates those as hierarchical blocks (`.asy` symbols). `LM324.lib` is the op-amp model and must stay next to the schematics. A second copy of `FinalParentSchematic.asc` sits at the repo root.
- `DECISIONS.md` is the design decision log (OPEN/DECIDED entries, each with "Needed by", "Owner", "Why" and "Rejected"). Many hardware/firmware parameters (clock source, fixed-point format, ADC reference, sampling and skew correction, filter corner) are tracked there. Check it before assuming a value.
- `setup-issues.sh` plus `.github/workflows/setup-issues.yml` generate the GitHub Issues task board (labels `wk:NN`, milestones). The workflow is manual (`workflow_dispatch`) and its modes include dry-run, create, sync and the destructive close-all/delete-all/wipe-labels.
- `Application/` is the Smart Energy Challenge "app": a PlatformIO project (board `esp32dev`, Arduino framework) for an ESP32-DevKitC-32E that runs a WiFi captive portal. The ATmega sends `$M`/`$W` text lines to UART2 (GPIO16 RX, GPIO17 TX, through a level shifter). `src/main.cpp` parses them and serves `web/index.html` plus JSON at `/api/data`, which the page polls. `Application/docs/spec.md` defines both formats, so change it together with `src/main.cpp` and `web/index.html`.
- The README layout also lists `pcb/`, `app/`, `enclosure/`, `test-data/` and `docs/`, which don't exist yet.

## Building firmware

Open Software/Joule_observer/Joule_observer.atsln in Atmel/Microchip Studio,
or run ./Software/build-and-test.ps1 from the repository root. The script
builds real and fixed-display firmware with warnings treated as errors and
runs capture/calculation/display/energy regression checks in the AVR instruction
simulator. Build outputs are under Debug/ and ignored by git.

The target is **ATmega328PB**, with a **2 MHz actual CPU clock**. ADC0/PC0 is
voltage, ADC1/PC1 is current, INT0/PD2 is the voltage zero crossing. USART0 TX
on PD1 sends $M readings (RMS current) and $D display modes at 9600 baud.
ADC prescaler /16 gives 104 us per conversion; Timer1 /64 gives 31250 Hz.
Keep clock, UART, ADC, Timer0 and energy constants consistent.

See Software/README.md for triage findings, raw ADC diagnostics and the
DISPLAY_SELF_TEST build. Keep only this AVR project.

## Building the ESP32 dashboard

PlatformIO isn't on PATH in a plain shell. Use `~/.platformio/penv/Scripts/pio.exe`, run from `Application/`:

```bash
pio run -e esp32dev            # real data from the ATmega on UART2
pio run -e mock                # MOCK_ATMEGA: the ESP32 fakes the ATmega's lines through the same parser
pio run -e mock -t upload      # flash; `pio device monitor` echoes every UART line at 115200
```

- `scripts/compress_page.py` creates deterministic `web/index.html.gz` before PlatformIO embeds it. Symbols are `_binary_web_index_html_gz_start` and `_end`; send its explicit length with Content-Encoding: gzip. No filesystem upload or external web assets are required.
- There are no lib_deps. WiFi, WebServer (synchronous) and DNSServer ship with the Arduino core (2.0.17 on `espressif32@7.0.1`). Everything runs from `loop()`, so there's no locking around the shared readings.
- To check page changes without hardware, open `web/index.html` from disk. On `file:` (or with `?demo`), it generates its own data in the `/api/data` shape.
- Run `node Application/tests/dashboard.test.cjs` for dashboard/calibration/state tests and `./Software/build-and-test.ps1` for AVR regression and ADC cycle-budget checks. Calibration gains/reference live in `Software/Joule_observer/Joule_observer/calibration.h`; power scaling is derived from both gains. Dashboard downloads require rebuilding/reflashing AVR, not remote writes.

## Team conventions (from README and team-charter.md)

- Commit message format: `area: what changed`, e.g. `fw: fix ADC mux settling` or `pcb: add Kelvin pads to shunt`.
- Commits are assessed per person: each member commits under their own account. Don't commit or push unless asked.
- Binary design files (Altium, LTspice `.asc/.asy`, Proteus `.pdsprj`, CAD) can't be merged, so don't edit them casually. They have a single owner and no branches. Firmware and scripts are text: branch and PR as usual.
- Record any design decision in `DECISIONS.md` the same day it's made, in the existing entry format.
- Tags mark assessed checkpoints: `progress-review`, `pcb-submission`, `final-demo`.
- Generated simulation and build output (`*.raw`, `*.log`, `*.net`, `Debug/`, `*.elf`/`*.hex`, and so on) is gitignored on purpose. Don't commit it. The `Uart testing/` folder at the root is ignored build output.
