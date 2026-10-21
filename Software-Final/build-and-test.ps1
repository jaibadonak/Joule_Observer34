param(
    [string]$Toolchain = 'C:/Program Files (x86)/Atmel/Studio/7.0/toolchain/avr8/avr8-gnu-toolchain/bin',
    [string]$DevicePack = 'C:/Program Files (x86)/Atmel/Studio/7.0/packs/atmel/ATmega_DFP/1.7.374',
    [string]$Python = "$env:USERPROFILE/.platformio/penv/Scripts/python.exe"
)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $project = 'JouleObserverFinal/JouleObserverFinal'
    New-Item -ItemType Directory -Force "$project/Debug", 'tests/Debug' | Out-Null
    $sources = Get-ChildItem "$project/*.c" | ForEach-Object FullName
    $options = @('-mmcu=atmega328pb', "-B$DevicePack/gcc/dev/atmega328pb/",
                 "-I$DevicePack/include", "-I$project", '-DF_CPU=2000000UL', '-Os', '-Wall',
                 '-Wextra', '-Werror', '-std=gnu99')
    foreach ($build in @('JouleObserverFinal', 'display-test')) {
        $buildSources = $sources
        if ($build -eq 'display-test') {
            $buildSources = @($sources | Where-Object { (Split-Path $_ -Leaf) -ne 'main.c' })
            $buildSources += 'tests/display-test.c'
        }
        & "$Toolchain/avr-gcc.exe" @options @buildSources -o "$project/Debug/$build.elf"
        if ($LASTEXITCODE) { throw "$build compilation failed" }
        & "$Toolchain/avr-objcopy.exe" -O ihex -R .eeprom "$project/Debug/$build.elf" "$project/Debug/$build.hex"
        if ($LASTEXITCODE) { throw "$build HEX generation failed" }
        & "$Toolchain/avr-size.exe" "$project/Debug/$build.elf"
    }
    & $Python ../Software/tests/check-isr-budget.py "$Toolchain/avr-objdump.exe" "$project/Debug/JouleObserverFinal.elf"
    if ($LASTEXITCODE) { throw 'ADC interrupt cycle budget failed' }

    @'
set confirm off
target sim
load
break main
run
break test_finished
continue
printf "Final regression failures: %u\n", test_failures
if test_failures != 0
  print failure_lines
  quit 1
end
quit 0
'@ | Set-Content -LiteralPath tests/Debug/regression.gdb
    foreach ($polarity in @('normal', 'inverted')) {
        [string[]]$extra = @()
        if ($polarity -eq 'inverted') { $extra = @('-DTEST_INVERT_CURRENT') }
        & "$Toolchain/avr-gcc.exe" -mmcu=atmega328p -g -Os -Wall -Wextra -Werror -std=gnu99 @extra tests/regression.c -lm -o tests/Debug/regression.elf
        if ($LASTEXITCODE) { throw 'Final calculation tests did not compile' }
        Write-Output "Checking $polarity current polarity"
        $simulatorOutput = & "$Toolchain/avr-gdb.exe" -q -batch tests/Debug/regression.elf -x tests/Debug/regression.gdb 2>&1
        $simulatorOutput | Write-Output
        if ($LASTEXITCODE -or -not ($simulatorOutput -match '^Final regression failures: 0$')) {
            throw 'Final calculation tests failed'
        }
    }
} finally {
    Pop-Location
}
