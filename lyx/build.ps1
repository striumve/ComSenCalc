<#
    build.ps1 -- build the firmware without CMake/Ninja.

    The project is normally built with `cmake --preset Debug`, but CMake and
    Ninja are not installed on this machine. This script calls arm-none-eabi-gcc
    directly with the same flags, include paths and defines the CMake toolchain
    file uses, so it produces the same output.

    Usage:
        pwsh -File build.ps1
        pwsh -File build.ps1 -Clean

    Output: build_manual/software.elf  .bin  .hex  .map
#>
param(
    [switch]$Clean,
    [string]$BuildDir = "build_manual",
    [string]$ToolchainBin = "D:\Software\ArmGNUToolchain\bin"
)

$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$out  = Join-Path $root $BuildDir
$obj  = Join-Path $out "obj"
$elf  = Join-Path $out "software.elf"
$bin  = Join-Path $out "software.bin"
$hex  = Join-Path $out "software.hex"
$map  = Join-Path $out "software.map"

$GCC     = Join-Path $ToolchainBin "arm-none-eabi-gcc.exe"
$OBJCOPY = Join-Path $ToolchainBin "arm-none-eabi-objcopy.exe"
$SIZE    = Join-Path $ToolchainBin "arm-none-eabi-size.exe"

foreach ($tool in @($GCC, $OBJCOPY, $SIZE)) {
    if (-not (Test-Path $tool)) { throw "toolchain not found: $tool" }
}

if ($Clean -and (Test-Path $out)) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force -Path $obj | Out-Null

# --- include paths (mirrors MX_Include_Dirs) -------------------------------
$includes = @(
    "Core/Inc"
    "USB_DEVICE/App"
    "USB_DEVICE/Target"
    "Drivers/STM32F1xx_HAL_Driver/Inc"
    "Drivers/STM32F1xx_HAL_Driver/Inc/Legacy"
    "Middlewares/Third_Party/FreeRTOS/Source/include"
    "Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2"
    "Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3"
    "Middlewares/ST/STM32_USB_Device_Library/Core/Inc"
    "Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc"
    "Drivers/CMSIS/Device/ST/STM32F1xx/Include"
    "Drivers/CMSIS/Include"
) | ForEach-Object { "-I" + (Join-Path $root $_) }

# --- C sources -------------------------------------------------------------
$cSources = @()
$cSources += Get-ChildItem (Join-Path $root "Core/Src") -Filter *.c | ForEach-Object { $_.FullName }
$cSources += Get-ChildItem (Join-Path $root "USB_DEVICE") -Recurse -Filter *.c | ForEach-Object { $_.FullName }
$cSources += Get-ChildItem (Join-Path $root "Drivers/STM32F1xx_HAL_Driver/Src") -Filter *.c | ForEach-Object { $_.FullName }
$cSources += Get-ChildItem (Join-Path $root "Middlewares/ST/STM32_USB_Device_Library") -Recurse -Filter *.c | ForEach-Object { $_.FullName }

# FreeRTOS: list explicitly -- the tree contains several ports and heap schemes
# and only these ten belong to this project.
$freertos = @(
    "croutine.c"
    "event_groups.c"
    "list.c"
    "queue.c"
    "stream_buffer.c"
    "tasks.c"
    "timers.c"
    "CMSIS_RTOS_V2/cmsis_os2.c"
    "portable/MemMang/heap_4.c"
    "portable/GCC/ARM_CM3/port.c"
) | ForEach-Object { Join-Path $root ("Middlewares/Third_Party/FreeRTOS/Source/" + $_) }
$cSources += $freertos

$cSources = $cSources | Sort-Object -Unique

# --- flags -----------------------------------------------------------------
$commonFlags = @("-mcpu=cortex-m3", "-mthumb", "-Og", "-g3", "-Wall",
                 "-fdata-sections", "-ffunction-sections",
                 "-DUSE_HAL_DRIVER", "-DSTM32F103xB", "-DDEBUG")

# --- compile ---------------------------------------------------------------
# Native tools write warnings to stderr. With ErrorActionPreference = 'Stop'
# PowerShell would turn those into a terminating error, so relax it here and
# rely on $LASTEXITCODE instead.
$ErrorActionPreference = "Continue"

$failed = @()
foreach ($src in $cSources) {
    $rel  = $src.Substring($root.Length).TrimStart('\', '/')
    $name = ($rel -replace '[\\/]', '_') -replace '\.c$', '.o'
    $dst  = Join-Path $obj $name

    Write-Host ("  CC  " + $rel)
    & $GCC @commonFlags @includes -c $src -o $dst
    if ($LASTEXITCODE -ne 0) { $failed += $rel }
}

# startup file is assembler-with-cpp
$startupSrc = Join-Path $root "startup_stm32f103xb.s"
$startupObj = Join-Path $obj "startup_stm32f103xb.o"
Write-Host "  AS  startup_stm32f103xb.s"
& $GCC @commonFlags @includes -x assembler-with-cpp -c $startupSrc -o $startupObj
if ($LASTEXITCODE -ne 0) { $failed += "startup_stm32f103xb.s" }

if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "FAILED to compile:" -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}

# --- link ------------------------------------------------------------------
$objects = Get-ChildItem $obj -Filter *.o | ForEach-Object { $_.FullName }

$linkFlags = @("-mcpu=cortex-m3", "-mthumb",
               ("-T" + (Join-Path $root "STM32F103xx_FLASH.ld")),
               "--specs=nano.specs",
               ("-Wl,-Map=" + $map),
               "-Wl,--gc-sections",
               "-Wl,--print-memory-usage")

$libs = @(
    (Join-Path $root "lib/libtouch_filter.a")
    (Join-Path $root "lib/libtouch_model.a")
    (Join-Path $root "lib/libcalculator_engine.a")
    "-lm"
)

Write-Host ""
Write-Host "  LD  software.elf"
& $GCC @linkFlags @objects @libs -o $elf
if ($LASTEXITCODE -ne 0) { Write-Host "LINK FAILED" -ForegroundColor Red; exit 1 }

# --- artifacts -------------------------------------------------------------
& $OBJCOPY -O binary $elf $bin
& $OBJCOPY -O ihex   $elf $hex

Write-Host ""
& $SIZE $elf
Write-Host ""
Write-Host "Built:" -ForegroundColor Green
Write-Host "  $elf"
Write-Host "  $bin"
Write-Host "  $hex"
