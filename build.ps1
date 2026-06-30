# build.ps1 -- build ZzFX.pdx for BOTH the Simulator and the Playdate device.
#
#   powershell -ExecutionPolicy Bypass -File build.ps1
#
# Produces ZzFX.pdx at the project root containing:
#   main.pdz   (compiled Lua)
#   pdex.dll   (simulator binary, x86)
#   pdex.bin   (device binary, ARM -- pdc converts pdex.elf to this)
#
# Machine-specific paths (adjust if your install differs):
$SDK   = "C:\Program Files (x86)\Playdate"                                    # SDK v3.0.6
$ARMBIN = "C:\dev\arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi\bin" # Arm GNU Toolchain (arm-none-eabi)
$VSCM  = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake"
$cmake = "$VSCM\CMake\bin\cmake.exe"
$ninja = "$VSCM\Ninja\ninja.exe"
$pdc   = "$SDK\bin\pdc.exe"

$env:PLAYDATE_SDK_PATH = $SDK
$env:PATH = "$ARMBIN;" + $env:PATH

# The Simulator locks ZzFX.pdx/pdex.dll; close it so pdc can overwrite.
Get-Process PlaydateSimulator -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600

# 1. Device (ARM) build -> copies pdex.elf into Source/
if (-not (Test-Path build_device)) {
    & $cmake -S . -B build_device -G Ninja -DCMAKE_MAKE_PROGRAM="$ninja" `
        -DCMAKE_TOOLCHAIN_FILE="$SDK\C_API\buildsupport\arm.cmake"
}
& $cmake --build build_device
if ($LASTEXITCODE -ne 0) { Write-Error "device build failed"; exit 1 }

# 2. Simulator build -> copies pdex.dll into Source/ (its own pdc post-step also
#    bundles the pdx; we re-run pdc below to be sure both binaries are included).
if (-not (Test-Path build)) {
    & $cmake -S . -B build -G "Visual Studio 17 2022"
}
& $cmake --build build --config Release   # post-step pdc may warn; ignored

# 3. Bundle Source/ (Lua + pdex.dll + pdex.elf) into ZzFX.pdx
& $pdc -sdkpath "$SDK" Source ZzFX.pdx
if ($LASTEXITCODE -ne 0) { Write-Error "pdc failed"; exit 1 }

Write-Host "OK -- ZzFX.pdx built (simulator + device)."
Write-Host "Open it in the Simulator, or Device -> Upload Game to Device."
