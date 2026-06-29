# ZzFX for Playdate — project context

Port of ZzFX (Frank Force's tiny JS sound-effect synth) to the Playdate.
Sounds are synthesized live from parameters — no audio files.

## Architecture (decided & implemented)
- The Playdate can't write raw PCM from Lua, so the synth lives in **C**.
  `src/zzfx.c` is a faithful port of `ZZFX.buildSamples`, producing a 16-bit
  PCM buffer at 44100 Hz (Playdate's native rate, no resampling).
- The buffer is played via `playdate->sound->sample->newSampleFromData(...,
  shouldFreeData=1)` through a pool of 16 `SamplePlayer`s.
- `src/main.c` registers everything on `kEventInitLua` (no C update callback,
  so Lua's `playdate.update` drives the demo).
- `Source/zzfx.lua` wraps the C-registered global `__zzfx(...)` as
  `zzfx({...})` (ZzFX array format; nils → defaults). Also `zzfxGetNote`.
- `Source/main.lua` is the demo (buttons + crank play sounds).

## Verification already done
- Synth output compared sample-for-sample against the real ZzFX `buildSamples`
  run in Node (randomness off): max abs diff ~5e-16 on a [-1,1] signal across a
  simple sound and a feature-heavy one (saw, shapeCurve, slide, deltaSlide,
  pitchJump, repeat/tremolo, modulation, bitcrush, delay, decay, biquad filter).
- `zzfx.c` + `main.c` compile clean with `-Wall -Wextra` (against a stubbed
  pd_api.h). Real SDK build not yet run.

## Build files
- `Makefile` — SDK `common.mk` route (Mac/Linux or MinGW).
- `CMakeLists.txt` — recommended on Windows (Visual Studio / VS Code).

## BUILD STATUS (simulator build RESOLVED 2026-06-03)
`ZzFX.pdx` is produced at the project root (contains `main.pdz`, `pdex.dll`,
`pdxinfo`).

Root cause of the old blocker: `PLAYDATE_SDK_PATH` was unset and the SDK is not
in the default `Documents\PlaydateSDK` fallback location, so `playdate_game.cmake`
aborted with "SDK Path not found". It was never a missing SDK.

Working recipe (plain PowerShell — NO Developer Command Prompt needed; the VS17
generator locates MSVC itself, and cmake ships inside Visual Studio):
```powershell
$env:PLAYDATE_SDK_PATH = "C:\Program Files (x86)\Playdate"   # v3.0.6 — the canonical SDK
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S . -B build -G "Visual Studio 17 2022"
& $cmake --build build --config Release
```
Note: 3 SDKs are installed on this machine — use `C:\Program Files (x86)\Playdate`
(v3.0.6). The others are stale: `C:\dev\FrankGames\playdate\SDK` (0.11.1) and
`C:\dev\reference\PlaydateSDK` (0.10.1, no simulator).

## STILL PENDING
1. Confirm it runs / sounds correct in the Playdate Simulator (open `ZzFX.pdx`).
2. On-device build (`make device` / armgcc toolchain) not yet attempted.

## Tuning note
ZzFX applies its 0.3 master volume twice (faithful to the JS lib) → quiet on the
Playdate speaker. Raise `ZZFX_PLAYBACK_VOLUME` in `src/zzfx.c` if needed.
