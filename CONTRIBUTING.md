# Contributing

Thanks for your interest in improving ZzFX for Playdate.

## Setup

1. Install the Playdate SDK.
2. Set PLAYDATE_SDK_PATH to your SDK location.
3. Build with either:
   - make (simulator)
   - cmake -S . -B build -G "Visual Studio 17 2022" and then cmake --build build --config Release

## Pull requests

1. Keep changes focused and small.
2. Update README.md and CHANGELOG.md when behavior or public usage changes.
3. Include clear reproduction steps for bug fixes.
4. Verify simulator build succeeds before opening a PR.

## Style

- C: keep changes warning-clean under -Wall -Wextra where practical.
- Lua: prefer clear parameter naming and short helper functions.
- Preserve behavior parity with upstream ZzFX unless a change is intentional and documented.
