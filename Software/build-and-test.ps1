param(
    [string]$Toolchain = 'C:/Program Files (x86)/Atmel/Studio/7.0/toolchain/avr8/avr8-gnu-toolchain/bin',
    [string]$DevicePack = 'C:/Program Files (x86)/Atmel/Studio/7.0/packs/atmel/ATmega_DFP/1.7.374',
    [string]$Python = "$env:USERPROFILE/.platformio/penv/Scripts/python.exe"
)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $project = 'Joule_observer/Joule_observer'
    New-Item -ItemType Directory -Force "$project/Debug", 'tests/Debug' | Out-Null
    $sources = Get-ChildItem "$project/*.c" | ForEach-Object FullName
    $options = @('-mmcu=atmega328pb', "-B$DevicePack/gcc/dev/atmega328pb/",
                 "-I$DevicePack/include", '-DF_CPU=2000000UL', '-Os', '-Wall',
                 '-Wextra', '-Werror', '-std=gnu99')
    foreach ($build in @('Joule_observer','display-test')) {
        [string[]]$extra = @()
        if ($build -eq 'display-test') { $extra = @('-DDISPLAY_SELF_TEST') }
        & "$Toolchain/avr-gcc.exe" @options @extra @sources -o "$project/Debug/$build.elf"
        if ($LASTEXITCODE) { throw "$build compilation failed" }
        & "$Toolchain/avr-objcopy.exe" -O ihex -R .eeprom "$project/Debug/$build.elf" "$project/Debug/$build.hex"
        if ($LASTEXITCODE) { throw "$build HEX generation failed" }
        & "$Toolchain/avr-size.exe" "$project/Debug/$build.elf"
    }
    & $Python tests/check-isr-budget.py "$Toolchain/avr-objdump.exe" "$project/Debug/Joule_observer.elf"
    if ($LASTEXITCODE) { throw 'ADC interrupt cycle budget failed' }
    # Separate binaries keep transport fixtures away from the 328P's 2 KB
    # measurement buffers/float-test stack. Both use production C code.
    foreach ($testName in @('regression','protocol-regression')) {
        & "$Toolchain/avr-gcc.exe" -mmcu=atmega328p -g -Os -Wall -Wextra -Werror -std=gnu99 "tests/$testName.c" -lm -o "tests/Debug/$testName.elf"
        if ($LASTEXITCODE) { throw "$testName compilation failed" }
        & "$Toolchain/avr-objcopy.exe" -O binary --only-section=.data "tests/Debug/$testName.elf" "tests/Debug/$testName-data.bin"
        if ($LASTEXITCODE) { throw "$testName data extraction failed" }
        $gdbCommands = @'
set confirm off
target sim
load
break main
run
restore tests/Debug/TEST-data.bin binary 0x800100
break test_finished
continue
printf "Regression failures: %u\n", test_failures
SUMMARY
if test_failures != 0
  print failure_lines
  quit 1
end
quit 0
'@
        $summary = ''
        if ($testName -eq 'regression') {
            $summary = 'printf "Specification grid: %u cases; max errors: V=%lu mV, Ipeak=%lu hundredths mA, P=%lu mW\n", spec_cases, spec_voltage_error_mV, spec_peak_error_mA_x100, spec_power_error_mW'
        }
        $gdbCommands.Replace('TEST',$testName).Replace('SUMMARY',$summary) | Set-Content -LiteralPath "tests/Debug/$testName.gdb"
        & "$Toolchain/avr-gdb.exe" -q -batch "tests/Debug/$testName.elf" -x "tests/Debug/$testName.gdb"
        if ($LASTEXITCODE) { throw "$testName checks failed" }
    }
} finally {
    Pop-Location
}
