#!/usr/bin/env bash
# EE209 Team [XX] - create labels, milestones and issues, in time order.
#
#   gh auth login
#   cd <clone of the TEAM repo>
#   bash setup-issues.sh --dry-run     # prints the plan week by week
#   bash setup-issues.sh               # actually creates them
#
# Fill in the four usernames first. Re-running creates duplicates.
#
# THERE IS ONE BOARD. Labs 1 and 3 hand you the whole analogue circuit;
# the challenge changes packages to SMT and adds the MCU section the
# Xplained Mini was providing. Same schematic either way. One submission,
# Thu 17 Sep. You do not build a TH board and then an SMT board.
#
# Every card carries a wk:NN label. In the Projects board, group by that
# label and the board reads as a calendar. Milestones give the coarse gates.

set -uo pipefail

# ---- who's who (GitHub usernames, no @) --------------------------
AN=""     # Jai   - analogue + PCB
FW=""     #       - DSP + core firmware
PE=""     #       - peripherals, comms, app
TE=""     #       - integration, test, enclosure
ALL="$AN,$FW,$PE,$TE"

DRY=0; [ "${1:-}" = "--dry-run" ] && DRY=1
declare -A TALLY; TOTAL=0

mk() {  # mk <wk> <milestone> <labels> <assignees,csv> <title> [body]
  local wk="$1" ms="$2" lb="$3" as="$4" ti="$5" bd="${6:-}"
  [ -z "$bd" ] && bd="Reasoning and context: see the team charter."
  bd="$bd"$'\n\n'"_Target: $wk. Milestone: $ms._"
  local args=(--title "$ti" --milestone "$ms" --label "$lb,$wk" --body "$bd")
  IFS=',' read -ra who <<< "$as"
  for w in "${who[@]}"; do
    [ -n "$w" ] && args+=(--assignee "$w") && TALLY[$w]=$(( ${TALLY[$w]:-0} + 1 ))
  done
  TOTAL=$((TOTAL+1))
  if [ "$DRY" = 1 ]; then printf '   %s\n' "$ti"
  else gh issue create "${args[@]}" >/dev/null && echo "   + $ti"; fi
}
wk() { echo; echo "### $*"; }

if [ "$DRY" = 0 ]; then
  command -v gh >/dev/null || { echo "gh CLI not found"; exit 1; }
  gh repo view >/dev/null 2>&1 || { echo "not inside a GitHub repo"; exit 1; }
  echo "== labels =="
  for l in "area:analogue|1d76db" "area:firmware|0e8a16" "area:pcb|5319e7" \
           "area:app|fbca04" "area:test|d93f0b" "area:admin|c5def5" \
           "area:emulator|0052cc" "blocked|b60205" "decision|e99695" \
           "assessed|f9d0c4" "everyone|bfd4f2" "as-taught|c2e0c6" "improvement|d4c5f9" \
           "wk02|ededed" "wk03|ededed" "wk04|ededed" "wk05|ededed" "wk06|ededed" \
           "brk|ededed" "wk07|ededed" "wk08|ededed" "wk09|ededed" "wk10|ededed" \
           "wk11|ededed" "wk12|ededed"; do
    gh label create "${l%%|*}" --color "${l##*|}" --force >/dev/null 2>&1
  done
  echo "== milestones =="
  for m in "progress-review|2026-08-25" "analogue-freeze|2026-09-01" \
           "pcb-submission|2026-09-17" "firmware-complete|2026-09-25" \
           "bring-up|2026-10-02" "characterisation|2026-10-16" "final-demo|2026-10-20"; do
    gh api "repos/{owner}/{repo}/milestones" -f title="${m%%|*}" \
       -f due_on="${m##*|}T00:00:00Z" >/dev/null 2>&1
  done
fi

# =================================================================
wk "WEEK 2  |  27-31 Jul  |  get unblocked"
mk wk02 progress-review "area:admin,everyone,assessed" "$ALL" "Safety induction completed" \
  "Mandatory before touching the MDLS labs. Blocks every practical task, so it goes first."
mk wk02 progress-review "area:admin,everyone" "$ALL" "Accept BOTH GitHub Classroom invitations" \
  "One creates your individual lab repo, one creates the team repo. Missing the individual one leaves your lab work nowhere to live."
mk wk02 progress-review "area:admin,everyone" "$ALL" "Slack and GitHub accounts on UoA email" \
  "Educational benefits only apply to the university address."
mk wk02 progress-review "area:admin,everyone,assessed" "$ALL" "Lecture quizzes, all 15 on Canvas" \
  "Standing item. 0.5% each capped at 5%, run during lectures Tue and Fri 12-2. Same weight as the entire progress review, for turning up."
mk wk02 progress-review "area:admin,everyone" "$ALL" "Toolchain installed: Atmel Studio, LTspice, Proteus VSM" \
  "Proteus needs a personal copy, instructions on Canvas. Done before the week 3 instrumentation workshop."
mk wk02 progress-review "area:admin,everyone" "$ALL" "Charter read and signed by all four"
mk wk02 progress-review "area:admin" "$TE" "Confirm team registered on Canvas, People > Groups" \
  "Was due end of week 1. Unregistered singles get randomly paired."
mk wk02 progress-review "area:admin" "$TE" "Repo scaffolded: README, .gitignore, DECISIONS.md, folders"
mk wk02 progress-review "area:admin" "$TE" "Projects board live, grouped by wk label"
mk wk02 progress-review "area:admin" "$PE" "Slack GitHub integration (/github subscribe)"
mk wk02 progress-review "area:admin,assessed" "$AN" "Pull the PCB Design Rules doc from Canvas" \
  "Blocks all layout. Chase a TA if it isn't posted yet."
mk wk02 progress-review "area:admin,assessed" "$AN" "Get the Component Dispensary stocked parts list" \
  "Blocks ALL library work. The Canvas SMT datasheet folder is a good start but appears to be scrolled, so confirm the full list."
mk wk02 progress-review "area:analogue" "$FW" "Pull the LTspice template from GitHub" \
  "Contains the device models for our parts. Don't start simulating without it."
mk wk02 analogue-freeze "area:analogue,decision" "$TE" "URGENT: power budget, the LM78L05 is the 100 mA part" \
  "Rectified 14 Vrms gives an 18.4 V rail, so the regulator drops 13.4 V. At 100 mA that is 1.34 W, roughly a 160 C rise in SOT-89. At 50 mA still 80 C. ESP32 WiFi TX peaks of 250-500 mA are not survivable at all. Options: BLE not WiFi, pre-drop resistor or BZT52H zener sharing the dissipation, separate supply for the ESP32, or all three. This constrains the whole design so it needs answering now. Review: analogue owner."

mk wk02 progress-review "area:admin,everyone,assessed" "$ALL" "LAB 1 assessment: AC Circuits, Thu 30 / Fri 31 Jul" \
  "Individually interviewed. 1.5% understanding, 0.5% logbook and GitHub and Slack, 0.5% progress. Push your individual lab repo before you walk in."

# =================================================================
wk "WEEK 3  |  3-7 Aug  |  decisions and library"
mk wk03 progress-review "area:admin,everyone" "$ALL" "Instrumentation workshop, 4-5 Aug"
mk wk03 analogue-freeze "area:analogue,decision" "$AN" "DECIDE: op-amp, LT6221 for the signal chain" \
  "LM324 on a single 5 V supply only swings to about V+ minus 1.5, so 3.5 V, with the same input common mode limit. The emulator A0 setting peaks near 4.1 V and would clip. LT6221 is rail to rail in and out. Keep the LM324 for the zero-cross comparator where headroom doesn't matter."
mk wk03 analogue-freeze "area:analogue,as-taught" "$AN" "Shunt value from Lab 1 Q3.1, then its SMT implementation" \
  "Lab caps shunt dissipation at 200 mW. Max current is 595 mA RMS (7.5 VA at 12.6 V), so 0.5 ohm gives 177 mW and 421 mV peak. Lab suggests two 1 ohm resistors in parallel, which carries straight to SMT: two ERJ-6ENF 1R 0805 in parallel is 0.5 ohm at 88 mW each, inside the 125 mW package limit. The parallel pair was always about splitting dissipation, not just hitting the value."
mk wk03 analogue-freeze "area:pcb,decision" "$PE" "DECIDE: USB-serial bridge part" \
  "CP2102, CH340 or whatever the dispensary stocks. The Xplained Mini gave us this for free, our board has to carry its own."
mk wk03 analogue-freeze "area:firmware,decision" "$FW" "DECIDE: clock source" \
  "No crystal appears in the Canvas SMT datasheet folder, which points at internal RC with CLKPR dividing 8 MHz to 2 MHz. RC drifts a few percent over temp and supply, which costs 9600 baud margin and makes the energy time base sloppy. Confirm whether the dispensary stocks a crystal before settling."
mk wk03 analogue-freeze "area:app,decision" "$PE" "DECIDE: BLE vs WiFi" \
  "Blocked by the power budget card. WiFi TX current cannot come from a 78L05."
mk wk03 analogue-freeze "area:firmware,decision" "$FW" "DECIDE: debugWIRE or ISP only" \
  "Probably already settled: the provided programmer is an AVRISP mkII, which is ISP only. Confirm nothing else is available before designing RESET."
mk wk03 analogue-freeze "area:pcb" "$FW" "Map USART1 pinout vs SPI0 and ISP conflict" \
  "The PB has two USARTs, which is why the challenge specifies it. USART1 overlaps SPI0 and the programming pins. Decide how the 595 gets clocked: hardware SPI or bit bang on spare GPIO."
mk wk03 analogue-freeze "area:pcb" "$FW" "Library: ATmega328PB TQFP32"
mk wk03 analogue-freeze "area:pcb" "$AN" "Library: LM4040-4.096"
mk wk03 analogue-freeze "area:pcb" "$AN" "Library: current shunt with 4-terminal Kelvin pads" \
  "Sense pads must tap inside the current path or trace resistance lands in the measurement."
mk wk03 analogue-freeze "area:pcb" "$PE" "Library: USB-serial bridge"
mk wk03 analogue-freeze "area:pcb" "$PE" "Library: ESP32 module with antenna keepout"
mk wk03 analogue-freeze "area:pcb" "$PE" "Library: 74HC595 display connector"
mk wk03 analogue-freeze "area:pcb" "$TE" "Library: LM78L05 with thermal pad, and the pre-drop part"
mk wk03 analogue-freeze "area:pcb" "$TE" "Library: S1A bridge, UWX1H electrolytic, BZT52H zener"
mk wk03 analogue-freeze "area:pcb" "$TE" "Library: passives set, CL21B 0805 ceramics, ERJ-6ENF 0805 resistors, LTST-C170 LED, 1N4148"
mk wk03 analogue-freeze "area:pcb,decision" "$AN" "SMT substitution pass: map every lab-derived value to a stocked SMT part" \
  "This IS the challenge pivot. The circuit does not change, the packages do. Walk the Lab 1 and Lab 3 schematic component by component: resistors to ERJ-6ENF 0805, ceramics to CL21B, electrolytics to UWX1H, rectifier to S1A, regulator to LM78L05, op-amps to LM324 or LT6221. Note the LM324 is a quad, so two differential amps plus an offset buffer plus the zero-cross comparator may fit in one package. Anything with no SMT equivalent in stock gets raised now, not in September."
mk wk03 analogue-freeze "area:pcb" "$AN" "Board outline within 20000 mm2, mounting holes, team number in silkscreen"
mk wk03 progress-review "area:emulator" "$PE" "Get the emulator board and EmulatorController.mlapp running" \
  "App Designer app, serial at 9600 on the emulator's own COM port."
mk wk03 progress-review "area:emulator" "$FW" "Document the emulator serial protocol in the repo" \
  "P0 to P5 sets phase angle in 10 degree steps, 0 to 50. A0 to A2 sets current channel amplitude to 3.25, 1.70, 1.15 Vpk-pk. Each command echoes back as an ack. ADC1 is the current channel."
mk wk03 progress-review "area:firmware,assessed" "$PE" "UART driver 9600 8N1, on Proteus and Xplained Mini" \
  "Worth 1% at the progress review outright. Start early, it is the one firmware item explicitly graded in week 6."

# =================================================================
wk "WEEK 4  |  10-14 Aug  |  analogue design"
mk wk04 analogue-freeze "area:emulator,decision" "$AN" "MEASURE emulator output DC bias before committing to the LM4040" \
  "A0 is 3.25 Vpk-pk. If the emulator biases at 2.5 V the waveform peaks at 4.125 V, which CLIPS against a 4.096 V reference. Our AREF plan and the emulator are incompatible unless the bias is lower. Scope it, then either pick the reference to suit or run AVCC while on the emulator."
mk wk04 progress-review "area:analogue,as-taught" "$AN" "Voltage divider, Lab 1 Q4.1, target 2 Vpk-pk at 15.4 Vrms" \
  "Ratio about 21.8. Answer Q4.3 on ohms vs kilo-ohms vs mega-ohms with real reasoning, it feeds the interview."
mk wk04 progress-review "area:analogue,as-taught" "$AN" "Two differential amplifiers, Lab 3 Q2.1 and Q3.1" \
  "Both channels: 2.1 V offset, 2 Vpk-pk at full load. Offset comes from the LM324 VOH/VOL midpoint on a 5 V rail, worked out in Lab 3 Q1.3 and Q1.4. Current channel needs gain of about 2.4 from the 0.84 Vpk-pk the shunt gives. Voltage channel gain is 1 because the divider already targets 2 Vpk-pk. Differential topology also rejects the common mode drop along the ground plane, which is the point of Q2.4."
mk wk04 progress-review "area:analogue,as-taught" "$AN" "Anti-alias filters, Lab 3 Part 4, and the per-channel Nyquist catch" \
  "Lab says put the corner below 5 kHz, which assumes the 10 kHz ADC rate. But we alternate V and I through one mux, so each channel is only sampled at about 4.8 kHz and the real Nyquist is 2.4 kHz. Design to 2.4 kHz, not 5 kHz. Worth raising with a TA, it is a genuinely good question and shows we understand what the mux does."
mk wk04 analogue-freeze "area:analogue,as-taught" "$TE" "Size Rin and check regulator dissipation, Lab 3 Part 5" \
  "The taught supply is half-wave rectifier plus current-limiting resistor Rin plus Cs plus linear regulator, so the pre-drop already exists in the design. Lab models 50 mA and asks what breaks at 100 mA. Our SMT part is the LM78L05, which is only 100 mA, so work out how the drop splits between Rin and the regulator and confirm the package survives it. Thermal management is a stated learning outcome and this is where to earn it."
mk wk04 progress-review "area:analogue,as-taught" "$TE" "5 V supply, Lab 3 Part 5: half-wave rectifier, Rin, Cs, regulator" \
  "Lab walks the whole thing including ripple from Cs and the line regulation sum. Answer Q5.4 on half wave vs full wave properly, it is an obvious interview question. Off the critical path, good first analogue block to own. Review: analogue owner."
mk wk04 progress-review "area:analogue" "$FW" "Zero-cross detector into ICP1" \
  "Lab 3 calls this the only component left for you to design on your own, so expect it to be probed in interviews harder than anything the labs walked us through. Square wave with edges on the Vac zero crossings, validated across the full supply range. LM324 is fine here, headroom does not matter for a comparator. Review: analogue owner."
mk wk04 progress-review "area:analogue,as-taught" "$AN" "2.1 V offset generator, Lab 3 Q5.5" \
  "Lab explicitly says a plain voltage divider is not good enough and hints at extra circuitry for a stable low-impedance reference. Both differential amplifiers depend on it, so a sloppy offset is a common mode error on every measurement."
mk wk04 analogue-freeze "area:pcb" "$AN" "ATmega328PB support circuitry: decoupling, reset, 6-pin ISP header"
mk wk04 analogue-freeze "area:pcb,decision" "$AN" "OPTIONAL: external ADC reference instead of AVCC" \
  "The taught design assumes 0 to 5 V off the regulator. Signals sit at 1.1 to 3.1 V (2.1 V offset, 2 Vpk-pk), so a 4.096 V LM4040 fits with room and stops full scale drifting with the 78L05 output tolerance, which is a systematic gain error on every reading. Modest resolution gain, real stability gain. It is a deviation from the taught path so decide deliberately and write it up. Check the emulator amplitude first, A0 is 3.25 Vpk-pk which reaches 3.7 V about a 2.1 V offset."
mk wk04 analogue-freeze "area:firmware" "$FW" "Brown-out detector config for EEPROM energy writes"
mk wk04 analogue-freeze "area:firmware,decision" "$FW" "DECIDE: fixed point Q format and overflow analysis" \
  "Show worst case headroom at 7.5 VA. Getting this wrong shows up as clipping only at full load."
mk wk04 progress-review "area:firmware" "$PE" "Proteus VSM model configured to match LTspice outputs"

mk wk06 analogue-freeze "area:analogue,improvement" "$AN" "IMPROVEMENT: lower the LM324 VOL" \
  "Lab 3 says simple techniques exist to pull VOL below the datasheet figure and encourages it. More headroom means more usable ADC range. USE IT OR LOSE IT: touches the schematic, so it must land before the 1 Sep freeze."
mk wk06 analogue-freeze "area:analogue,improvement" "$AN" "IMPROVEMENT: light-load current accuracy, Lab 3 Q2.4" \
  "At 2.5 VA and 15.4 V the current is only 162 mA, so Vio gets small and quantisation bites. Lab asks the question and leaves it open. USE IT OR LOSE IT: schematic change, must land before the 1 Sep freeze."
mk wk06 analogue-freeze "area:analogue,improvement" "$AN" "IMPROVEMENT: 2nd order anti-alias filter" \
  "Lab 3 shows how to configure a differential amplifier as a 1st order filter, so cascading two gives 2nd order with no extra op-amp. Directly fixes the 2.4 kHz corner problem: a 1st order filter cannot be both low enough to stop aliasing and high enough not to phase shift 500 Hz. USE IT OR LOSE IT: schematic change, must land before the 1 Sep freeze."

mk wk04 progress-review "area:admin,everyone,assessed" "$ALL" "LAB 2 assessment: UART, Tue 11 / Wed 12 Aug" \
  "Individually interviewed, 2.5%. Push your individual lab repo first."

# =================================================================
wk "WEEK 5  |  17-21 Aug  |  Test 1 + Altium + Lab 3 assessment, the worst week"
mk wk05 progress-review "area:admin,everyone,assessed" "$ALL" "Test 1, Mon 17 Aug" \
  "Tests are 45% of the course with a 40% average hurdle to pass. Note this week also has the Altium workshops on 18-19 and the Lab 3 assessment on 20-21, so it is the most loaded week of the semester. Front-load Lab 3 into week 4 so it is finished before Test 1."
mk wk05 progress-review "area:admin,everyone" "$ALL" "Altium workshops 18-19 Aug, all four attend" \
  "Everyone has footprints to draw, so nobody skips this."
mk wk05 progress-review "area:admin,everyone,assessed" "$ALL" "LAB 3 assessment: Signal Conditioning, Thu 20 / Fri 21 Aug" \
  "Individually interviewed, 2.5%. This is the analogue design lab, so it doubles as a dry run for the progress review. Note it lands in the SAME week as Test 1 and the Altium workshops."
mk wk05 pcb-submission "area:pcb" "$AN" "Schematic capture: MCU, power and digital blocks" \
  "Analogue left as a reserved placeholder area until the freeze."

# =================================================================
wk "WEEK 6  |  24-28 Aug  |  progress review"
mk wk06 progress-review "area:analogue,assessed" "$AN" "LTspice sweep: 14 V +/-10%, 500 Hz +/-2%, PF 0.6-0.99, 2.5-7.5 VA" \
  "Explicitly worth 2% at the progress review."
mk wk06 progress-review "area:analogue" "$PE" "Breadboard validation against LTspice" \
  "Everyone gets interviewed on the analogue, so it isn't the analogue owner who builds this one. Review: analogue owner."
mk wk06 progress-review "area:test" "$TE" "Extend the Matlab DSP script" \
  "For loop instead of the hardcoded realignment array. Run at 500 Hz not 50. Non-integer N=9.62. Frequency at both ends of +/-2%. Added noise. Distorted current waveform. Add Vrms and Irms, which the script never computes."
mk wk06 progress-review "area:test" "$TE" "Error curves vs PF and vs load, as % of FULL SCALE" \
  "The script reports % of reading, Table I specifies % of full scale. Measure against the one we're marked on."
mk wk06 progress-review "area:admin,everyone,assessed" "$ALL" "Peer assessment before the progress review" \
  "Marks can be withheld from the whole team if one person doesn't submit."
mk wk06 progress-review "area:admin" "$TE" "Logbook check, everyone current before the review"
mk wk06 progress-review "area:admin,assessed" "$AN" "Ask Thrimawithana for Smart Energy Challenge permission" \
  "Judged on the first six weeks. Go in with a part-built board, not a plan."

# =================================================================
wk "BREAK  |  31 Aug - 11 Sep  |  layout, support Tue and Thu"
mk brk analogue-freeze "area:analogue,decision" "$AN" "ANALOGUE FREEZE, DECISIONS.md updated" \
  "No schematic changes after this. Layout starts."
mk brk pcb-submission "area:pcb" "$AN" "Schematic capture: analogue front end"
mk brk pcb-submission "area:pcb" "$AN" "Layout: split analogue and digital ground pours, single join point"
mk brk pcb-submission "area:pcb" "$AN" "Layout: ESP32 and antenna keepout, well clear of the front end"
mk brk pcb-submission "area:pcb" "$AN" "Layout: guard traces on high impedance nodes"
mk brk pcb-submission "area:pcb" "$AN" "Layout: thermal copper for the regulator and pre-drop" \
  "Falls out of the week 2 power budget. This is where the calculation becomes copper."
mk brk pcb-submission "area:pcb" "$AN" "Design for test: test points on every analogue node, injection header, status LEDs" \
  "The injection header lets us feed a known signal into the conditioning chain without the sensor. Makes bring-up survivable."
mk brk pcb-submission "area:pcb" "$AN" "DRC clean against the PCB Design Rules"
mk brk pcb-submission "area:pcb" "$TE" "Independent board review before the TA check" \
  "Second pair of eyes on silkscreen, connector orientation, footprint pin 1, DFT coverage. One reversed part caught here saves the project."
mk brk pcb-submission "area:admin,assessed" "$AN" "TA check on the PCB, target ~8 Sep" \
  "A week early so there is slack to fix whatever they find."
mk brk pcb-submission "area:admin" "$PE" "BOM finalised, parts reserved at the dispensary (405.572)"

# =================================================================
wk "WEEK 7  |  14-18 Sep  |  PCB submission Thu 17"
mk wk07 pcb-submission "area:pcb" "$AN" "Submit, tag main as pcb-submission, copy gerbers to pcb/outputs/"
mk wk07 pcb-submission "area:admin,everyone" "$ALL" "TH assembly workshop"
mk wk07 firmware-complete "area:admin,everyone,assessed" "$ALL" "LAB 4 assessment: ADC, Tue 15 / Wed 16 Sep" \
  "Individually interviewed, 2.5%. Same week as PCB submission."
mk wk07 firmware-complete "area:firmware" "$FW" "ADC init: prescaler 16, external AREF, V/I mux alternation"
mk wk07 firmware-complete "area:firmware" "$FW" "Timer-driven sample scheduling"

# =================================================================
wk "WEEK 8  |  21-25 Sep  |  the DSP core"
mk wk08 bring-up "area:admin,everyone" "$ALL" "SMT assembly workshop" \
  "We are building an SMT board for the challenge, so this one is not optional."
mk wk08 firmware-complete "area:admin,everyone,assessed" "$ALL" "LAB 5 assessment: Timers, Tue 22 / Wed 23 Sep" \
  "Individually interviewed, 2.5%."
mk wk08 firmware-complete "area:firmware" "$FW" "Auto-zero: mean over an integer number of cycles, subtracted per window" \
  "Do NOT hardcode the offset the way the example script does. Real bias is unknown and drifts. This removes op-amp offset, bias mismatch and ADC offset in one go."
mk wk08 firmware-complete "area:firmware" "$FW" "Sampling skew: two-point average realignment for the power product" \
  "Naive multiplication reads HIGH because current is sampled after voltage, so the apparent phase lag comes out smaller. Table I says PF 0.6 but the real load (4 mH plus 5-105 ohm) only reaches about PF 0.80 inside the 2.5-7.5 VA window, which is +18.8% of reading. Still far outside spec. The emulator at 50 degrees is harsher than the real load can be, so test against that."
mk wk08 firmware-complete "area:firmware" "$FW" "cos(pi/N) gain correction on realigned power" \
  "Realignment leaves a flat -5.29% at N=9.62, identical at every PF. Uncorrected we fail spec at PF 0.99 and 7.5 VA, at -5.24% of full scale."
mk wk08 firmware-complete "area:firmware" "$FW" "Vrms and Irms from RAW samples, not realigned" \
  "Using realigned samples for RMS drags displayed peak current down by the same 5.29%. The realigned series is for the power product only."
mk wk08 firmware-complete "area:firmware" "$PE" "74HC595 display driver with 1 s scroll"
mk wk08 firmware-complete "area:app" "$PE" "ESP32 firmware, UART to BLE bridge"

# =================================================================
wk "WEEK 9  |  28 Sep - 2 Oct  |  boards land, bring-up"
mk wk09 bring-up "area:admin,everyone,assessed" "$ALL" "LAB 6 assessment: Displays, Tue 29 / Wed 30 Sep" \
  "Individually interviewed, 2.5%. Last one, and it collides with board bring-up."
mk wk09 bring-up "area:admin" "$TE" "Book soldering workshop time (405.521, 8-5 weekdays only)" \
  "Book the moment boards land. Evenings are not an option."
mk wk09 bring-up "area:pcb" "$TE" "Stage 1: solder power section ONLY, verify the 5 V rail and regulator temperature" \
  "Nothing else goes on until this measures right and stays cool. If the rail is wrong everything downstream dies."
mk wk09 bring-up "area:pcb" "$AN" "Stage 2: solder MCU and support, confirm ISP connectivity with the AVRISP mkII"
mk wk09 bring-up "area:firmware" "$FW" "Stage 3: set fuses, flash blinky, verify 2 MHz on a scope"
mk wk09 bring-up "area:pcb" "$PE" "Stage 4: solder comms, verify both UARTs and USB enumeration"
mk wk09 bring-up "area:pcb" "$AN" "Stage 5: solder the analogue front end"
mk wk09 bring-up "area:firmware" "$FW" "Port firmware from Xplained Mini to our board: pin remap, fuses, clock" \
  "Everything must already work on the emulator before boards arrive. Bring-up should only ever be chasing hardware."
mk wk09 bring-up "area:test" "$TE" "First light: board outputs compared against breadboard and LTspice"
mk wk09 firmware-complete "area:firmware" "$FW" "Zero-cross ISR on ICP1: frequency measurement and synchronous window" \
  "9.62 samples per cycle is non-integer, so a fixed window leaks and the leakage moves as source frequency drifts across +/-2%."
mk wk09 firmware-complete "area:firmware" "$FW" "Energy accumulation with EEPROM persistence and wear levelling"
mk wk09 firmware-complete "area:firmware" "$FW" "PF, VA, VAR and phase angle derived and reported"
mk wk09 firmware-complete "area:firmware" "$FW" "Two-point calibration constants in EEPROM, applied at startup"
mk wk09 firmware-complete "area:firmware" "$PE" "UART command set: cal read / write / restore defaults, documented"
mk wk09 firmware-complete "area:firmware" "$PE" "Power-on self test with error codes on the display"

# =================================================================
wk "WEEK 10  |  5-9 Oct  |  Test 2, integration continues"
mk wk10 characterisation "area:admin,everyone,assessed" "$ALL" "Test 2, Mon 5 Oct"
mk wk10 firmware-complete "area:firmware" "$FW" "Firmware quality pass: structure, comments, no magic numbers" \
  "Project deliverables mark quality of the hardware and software implementation at 5%, separately from whether it works."
mk wk10 bring-up "area:pcb" "$AN" "Hardware quality pass: solder joints, alignment, strain relief, legible silkscreen"

# =================================================================
wk "WEEK 11  |  12-16 Oct  |  characterisation and polish"
mk wk11 characterisation "area:emulator" "$TE" "Python emulator driver: automated sweep, 6 angles x 3 amplitudes" \
  "18 test points, fully scriptable over the echo-ack protocol. Turns firmware accuracy into a regression we re-run after every change."
mk wk11 characterisation "area:emulator" "$TE" "Note the emulator PF ceiling in the test plan" \
  "Emulator maxes at 50 degrees, PF 0.643. Table I says PF 0.6, but the real load cannot reach it inside the VA window either: worst real case is about PF 0.80 at 7.5 VA and 12.6 V. So the emulator is actually the harsher phase test and PF 0.6 is untestable on either. Say so in the test plan rather than quietly not covering it."
mk wk11 characterisation "area:test" "$TE" "Python UART harness: load sweep, log vs bench reference, auto-plot"
mk wk11 characterisation "area:test,assessed" "$TE" "Full accuracy characterisation across load and PF" \
  "Measurement accuracy is 5% of the project mark on its own."
mk wk11 characterisation "area:test" "$AN" "Noise floor measurement, before and after layout changes"
mk wk11 characterisation "area:test,assessed" "$TE" "Test data committed to test-data/"
mk wk11 characterisation "area:app" "$PE" "App: live V and I waveform traces" \
  "The phase shift moving as the load changes is the demo that lands, and it is the same shift the skew correction exists to measure."
mk wk11 characterisation "area:app" "$PE" "App: energy over time and cost readout in c/kWh"
mk wk11 characterisation "area:app" "$PE" "App: calibration UI, gated behind a physical button, factory restore" \
  "Real meters have sealed calibration for exactly this reason. Worth saying so unprompted in the interview."
mk wk11 characterisation "area:app" "$TE" "Enclosure CAD around the actual board, checked against the Altium 3D view"
mk wk11 characterisation "area:app" "$TE" "Print, fit check, display window and connector cutouts"

# =================================================================
wk "WEEK 12  |  19-23 Oct  |  interviews Tue to Thu"
mk wk12 final-demo "area:admin,everyone" "$AN" "Walkthrough: analogue front end and PCB, 20 min to the team"
mk wk12 final-demo "area:admin,everyone" "$FW" "Walkthrough: sampling, skew correction and the fixed point maths"
mk wk12 final-demo "area:admin,everyone" "$PE" "Walkthrough: UART, display, ESP32 and the app"
mk wk12 final-demo "area:admin,everyone" "$TE" "Walkthrough: power supply, test method and the accuracy results"
mk wk12 final-demo "area:admin,everyone,assessed" "$ALL" "Peer assessment before the final interview"
mk wk12 final-demo "area:admin,everyone" "$ALL" "Demo rehearsal, full run through with the test rig"
mk wk12 final-demo "area:admin" "$PE" "Final repo audit: LTspice, Atmel Studio, Proteus VSM, Altium, test data" \
  "The outline lists these explicitly as the project deliverable contents."
mk wk12 final-demo "area:admin" "$TE" "Tag main as final-demo"
mk wk12 final-demo "area:admin" "$TE" "Return the team toolkit to the Component Dispensary"

# =================================================================
echo
echo "=== workload split ==="
for k in "$AN|analogue+PCB" "$FW|firmware/DSP" "$PE|peripherals/app" "$TE|test/integration"; do
  u="${k%%|*}"; r="${k##*|}"
  printf '  %-14s %-20s %s\n' "${u:-<unset>}" "$r" "${TALLY[$u]:-0} issues"
done
echo "  total cards: $TOTAL"
