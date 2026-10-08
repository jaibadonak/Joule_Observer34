# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ELECTENG 209 (University of Auckland) Team 34 project, "Joule Observer": a smart energy monitor that measures voltage, current and power delivered to a variable AC load (14 Vrms, 500 Hz, 2.5–7.5 VA, PF 0.6–0.99), shows them on a local display and sends them over UART (9600 8N1: V, Ipk, P, Energy) to a phone. The team is entering the Smart Energy Challenge, which means one double-layer SMT PCB with its own ATmega328PB at a 2 MHz system clock (no Xplained Mini), plus a phone app and a 3D-printed enclosure. See the README for the full spec table, owners and dates.

Most of the repo is binary or tool-native design files (LTspice, Proteus, Altium, CAD). The hand-written source is the ATmega firmware (`firmware/`) and the ESP32 WiFi dashboard (`Application/`).

## Repo map

- `firmware/UART_209/` is a Microchip/Atmel Studio 7 solution (`.atsln` + `.cproj`) with a single `main.c`. It is currently a UART proof of concept: it sends hard-coded placeholder RMS voltage, peak current and power values once a second as fixed-point ASCII (the `print_*` helpers scale by 10 or 100 and print digit by digit, with no `printf` or float formatting). Real sampling and DSP are not written yet.
- `firmware/UART_SIM.pdsprj` is the Proteus VSM simulation of the firmware.
- `analogue/MASTER_SCHEMATIC_EE209.asc` and `LTspice-Final/` hold the LTspice models of the analogue front end: the signal conditioning (`sig_cond*`, first/second order, voltage and shunt-current channels), the zero-crossing detector, and `FinalParentSchematic.asc`, which instantiates those as hierarchical blocks (`.asy` symbols). `LM324.lib` is the op-amp model and must stay next to the schematics. A second copy of `FinalParentSchematic.asc` sits at the repo root.
- `DECISIONS.md` is the design decision log (OPEN/DECIDED entries, each with "Needed by", "Owner", "Why" and "Rejected"). Many hardware/firmware parameters (clock source, fixed-point format, ADC reference, sampling and skew correction, filter corner) are tracked there. Check it before assuming a value.
- `setup-issues.sh` plus `.github/workflows/setup-issues.yml` generate the GitHub Issues task board (labels `wk:NN`, milestones). The workflow is manual (`workflow_dispatch`) and its modes include dry-run, create, sync and the destructive close-all/delete-all/wipe-labels.
- `Application/` is the Smart Energy Challenge "app": a PlatformIO project (board `esp32dev`, Arduino framework) for an ESP32-DevKitC-32E that runs a WiFi captive portal. The ATmega sends `$M`/`$W` text lines to UART2 (GPIO16 RX, GPIO17 TX, through a level shifter). `src/main.cpp` parses them and serves `web/index.html` plus JSON at `/api/data`, which the page polls. `Application/docs/spec.md` defines both formats, so change it together with `src/main.cpp` and `web/index.html`.
- The README layout also lists `pcb/`, `app/`, `enclosure/`, `test-data/` and `docs/`, which don't exist yet.

## Building firmware

The real workflow is to open `firmware/UART_209/UART_209.atsln` in Atmel Studio 7 and build. To compile from the command line with the toolchain that ships with Atmel Studio (avr-gcc 5.4):

```bash
T="/c/Program Files (x86)/Atmel/Studio/7.0/toolchain/avr8/avr8-gnu-toolchain/bin"
"$T/avr-gcc" -mmcu=atmega328p -Os -Wall -o out.elf firmware/UART_209/UART_209/main.c
"$T/avr-size" out.elf
"$T/avr-objcopy" -O ihex -R .eeprom out.elf out.hex
```

Write build output outside the repo or under `Debug/`/`Release/`, which `.gitignore` excludes. There are no tests or linters. Verification happens in Proteus or on hardware.

Points to keep in mind when editing firmware:
- `F_CPU` is `#define`d as 2 MHz at the top of `main.c` (the course-mandated system clock), and `UBRR` is derived from it. At 2 MHz, 9600 baud gives UBRR = 12 (about 0.2% error). If you change the clock or the baud rate, recheck the error.
- The `.cproj` targets **ATmega328P**, but the final board uses the **ATmega328PB**. USART1 on the PB overlaps SPI0 and the ISP pins, which is an open decision in `DECISIONS.md`.
- The values are 10-bit ADC samples on a 2 MHz AVR, so avoid `double` and `printf` in real code paths. The fixed-point format is still an open decision.

## Building the ESP32 dashboard

PlatformIO isn't on PATH in a plain shell. Use `~/.platformio/penv/Scripts/pio.exe`, run from `Application/`:

```bash
pio run -e esp32dev            # real data from the ATmega on UART2
pio run -e mock                # MOCK_ATMEGA: the ESP32 fakes the ATmega's lines through the same parser
pio run -e mock -t upload      # flash; `pio device monitor` echoes every UART line at 115200
```

- `web/index.html` is built into the firmware by `board_build.embed_files`. Its symbols are `_binary_web_index_html_start` and `_end`, and it has no NUL terminator, so it's sent with an explicit length. Don't switch to `embed_txtfiles`, which rewrites the HTML file during the build.
- There are no lib_deps. WiFi, WebServer (synchronous) and DNSServer ship with the Arduino core (2.0.17 on `espressif32@7.0.1`). Everything runs from `loop()`, so there's no locking around the shared readings.
- To check page changes without hardware, open `web/index.html` from disk. On `file:` (or with `?demo`), it generates its own data in the `/api/data` shape.
- The repo has no automated tests. The page's states can be checked by serving it next to a fake `/api/data`.

## Team conventions (from README and team-charter.md)

- Commit message format: `area: what changed`, e.g. `fw: fix ADC mux settling` or `pcb: add Kelvin pads to shunt`.
- Commits are assessed per person: each member commits under their own account. Don't commit or push unless asked.
- Binary design files (Altium, LTspice `.asc/.asy`, Proteus `.pdsprj`, CAD) can't be merged, so don't edit them casually. They have a single owner and no branches. Firmware and scripts are text: branch and PR as usual.
- Record any design decision in `DECISIONS.md` the same day it's made, in the existing entry format.
- Tags mark assessed checkpoints: `progress-review`, `pcb-submission`, `final-demo`.
- Generated simulation and build output (`*.raw`, `*.log`, `*.net`, `Debug/`, `*.elf`/`*.hex`, and so on) is gitignored on purpose. Don't commit it. The `Uart testing/` folder at the root is ignored build output.
