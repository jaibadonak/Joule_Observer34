# Design Decisions

Every real decision gets an entry, written the day we make it. Takes two minutes and saves an argument in October.

The point of this file is the final interview. "Why did you choose X" gets asked, and this is how all four of us give the same answer about something we settled in August.

Anything still `OPEN` past its "needed by" date is a problem worth raising at standup.

---

## Worked example, delete once we have real ones

```
## DECIDED Shunt resistor value
Needed by:  before Lab 3, the differential amp gain depends on it
Owner:      Everyone
Decided:    0.5 ohm, made from two 1 ohm 0805 in parallel
Why:        Max current is 595 mA RMS at 7.5 VA and 12.6 V. Lab 1 caps shunt
            dissipation at 200 mW, and 0.5 ohm gives 177 mW. Splitting it
            across two 0805s puts 88 mW in each, inside the 125 mW package
            rating. Gives 421 mV peak to amplify.
Rejected:   1 ohm, better signal but 287 mW, over the lab limit and over the
            package rating even split two ways. Current transformer, no phase
            error correction needed but bigger, costlier and not in stock.
Date:       31/07/2026
```

---

# Analogue

## OPEN Shunt resistor value
Needed by:  before Lab 3, the differential amp gain depends on it
Owner:      Everyone
Decided:    0.5 ohm, made from two 1 ohm 0805 in parallel
Why:        Max current is 595 mA RMS at 7.5 VA and 12.6 V. Lab 1 caps shunt
            dissipation at 200 mW, and 0.5 ohm gives 177 mW. Splitting it
            across two 0805s puts 88 mW in each, inside the 125 mW package
            rating. Gives 421 mV peak to amplify.
Rejected:   1 ohm, better signal but 287 mW, over the lab limit and over the
            package rating even split two ways. Current transformer, no phase
            error correction needed but bigger, costlier and not in stock.
Date:       31/07/2026

## OPEN Voltage divider values
Needed by: week 4. Lab 1 Q4.1 targets 2 Vpk-pk at 15.4 Vrms.
Owner:

## OPEN Op-amp choice
Needed by: week 3. LT6221 confirmed usable for SMT pending stock. Rail to rail
but a dual not a quad, and higher supply current than the LM324. Option to mix.
Owner:

## OPEN Offset voltage
Needed by: week 4. Depends on the op-amp choice. 2.1 V is the as-taught answer
from LM324 headroom. Rail to rail lets us re-centre and use more ADC range.
Owner:

## OPEN ADC reference
Needed by: week 4. AVCC as taught, or an external LM4040. Only worth doing
together with the offset decision. Check the emulator amplitude first.
Owner:

## OPEN Anti-alias filter corner
Needed by: week 4. Lab says below 5 kHz. Alternating channels through one mux
makes the real per-channel Nyquist 2.4 kHz. Worth asking a TA.
Owner:

## OPEN First or second order filter
Needed by: before the analogue freeze, 1 Sep
Owner:

## OPEN Regulator: how the voltage drop gets shared
Needed by: week 4. Rin vs regulator dissipation, and whether the 78L05 survives
whatever current we end up drawing.
Owner:

## OPEN Zero-cross detector circuit
Needed by: before the analogue freeze. This is the one block the course leaves
entirely to us, so expect it to be probed hardest.
Owner:

# Firmware

## OPEN Clock source
Needed by: week 3. Crystal or internal RC. Affects baud margin and the energy
time base.
Owner:

## OPEN Fixed point format
Needed by: week 4. Show the worst case headroom at 7.5 VA.
Owner:

## OPEN Sampling scheme and skew correction
Needed by: week 7. Channel order, how we realign, and how we handle the
residual.
Owner:

## OPEN How the display gets clocked
Needed by: week 3. Hardware SPI or bit bang, given USART1 overlaps SPI0 and the
ISP pins.
Owner:

## OPEN Programming interface
Needed by: week 3. ISP only or debugWIRE too. Constrains what hangs off RESET.
Owner:

# Board

## OPEN USB to serial bridge part
Needed by: week 3
Owner:

## OPEN BLE or WiFi
Needed by: week 3. Drives the power budget.
Owner:

## OPEN Altium workspace or local files
Needed by: week 3, before library work starts. Four of us drawing footprints
into files that cannot be merged is the thing to avoid.
Owner:

## OPEN Enclosure approach
Needed by: week 8
Owner:

# Project

## OPEN Are we doing the Smart Energy Challenge
Needed by: week 6, needs the course director's permission
Owner:
