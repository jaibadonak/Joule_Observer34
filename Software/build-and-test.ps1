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
    # The instruction simulator supports the 328P instruction set shared by PB.
    & "$Toolchain/avr-gcc.exe" -mmcu=atmega328p -g -Os -Wall -Wextra -Werror -std=gnu99 tests/regression.c -lm -o tests/Debug/regression.elf
    if ($LASTEXITCODE) { throw 'Regression compilation failed' }
    & "$Toolchain/avr-objcopy.exe" -O binary --only-section=.data tests/Debug/regression.elf tests/Debug/data.bin
    if ($LASTEXITCODE) { throw 'Regression data extraction failed' }
    # This bundled simulator does not preserve the flash initializer image for
    # startup's .data copy. Restore the ELF's actual initial data after startup.
    @'
set confirm off
target sim
load
break main
run
restore tests/Debug/data.bin binary 0x800100
break test_finished
continue
printf "Regression failures: %u\n", test_failures
printf "Specification grid: %u cases; max errors: V=%lu mV, Ipeak=%lu hundredths mA, P=%lu mW\n", spec_cases, spec_voltage_error_mV, spec_peak_error_mA_x100, spec_power_error_mW
if test_failures != 0
  quit 1
end
quit 0
'@ | Set-Content -LiteralPath tests/Debug/check.gdb
    & "$Toolchain/avr-gdb.exe" -q -batch tests/Debug/regression.elf -x tests/Debug/check.gdb
    if ($LASTEXITCODE) { throw 'Regression checks failed' }
} finally {
    Pop-Location
}
