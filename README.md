# ZzFX for Playdate

[ZzFX](https://github.com/KilledByAPixel/ZzFX) — Frank Force's tiny sound-effect
synth — ported to the Playdate. Sounds are generated **live from code**, no audio
files. Call `zzfx({...})` from Lua with the same parameter arrays you design at
the [ZzFX sound designer](https://killedbyapixel.github.io/ZzFX/).

## How it works

The Playdate can't write raw PCM from Lua, so the synth lives in **C**: the
`buildSamples` algorithm is ported in `src/zzfx.c`, producing a 16-bit PCM buffer
at 44100 Hz (the Playdate's native rate — no resampling). The buffer is wrapped
with `playdate->sound->sample->newSampleFromData()` and played through a pool of
16 `SamplePlayer`s so sounds can overlap. A thin Lua shim (`Source/zzfx.lua`)
lets you call it the familiar way: `zzfx({1, nil, 220, ...})`.

The C synth is a faithful port: for the same parameters it produces samples
identical to the JavaScript library to within floating-point rounding
(verified sample-for-sample, max difference ~5e-16 on a [-1,1] signal).

## Usage from Lua

```lua
import "zzfx"

zzfx({nil, nil, 1675, nil, .06, .24, 1, 1.82, nil, nil, 837, .06})  -- a coin
zzfx({nil, nil, 220})                                                -- simple beep
local f = zzfxGetNote(7, 220)  -- frequency 7 semitones above 220 Hz
zzfx({nil, nil, f, nil, .04, .12})
```

### Pasting sounds from the ZzFX designer

The designer gives you JavaScript sparse arrays like:

```js
zzfx(...[,,1675,,.06,.24,1,1.82,,,837,.06]);
```

Those empty slots aren't valid Lua. Replace every gap (including leading commas)
with `nil`:

```lua
zzfx({nil,nil,1675,nil,.06,.24,1,1.82,nil,nil,837,.06})
```

Trailing parameters you don't use can just be dropped.

## Calling from C

```c
#include "zzfx.h"
// in kEventInitLua (or kEventInit): zzfx_init(pd);
double coin[21] = {1,.05,1675,0,.06,.24,1,1.82,0,0,837,.06,0,0,0,0,0,1,0,0,0};
zzfx_play(coin);
```

## Building

Requires the [Playdate SDK](https://play.date/dev/). Point `PLAYDATE_SDK_PATH`
at it (or have it in `~/.Playdate/config`).

```
make            # Simulator build  -> ZzFX.pdx
make device     # Playdate hardware build (needs arm-none-eabi-gcc)
```

Then open `ZzFX.pdx` in the Playdate Simulator, or sideload it to a device.

### Project layout

```
Makefile          uses the SDK's common.mk (combined C + Lua build)
src/
  zzfx.h / zzfx.c the C synth + Lua binding (registers global __zzfx)
  main.c          eventHandler: zzfx_init + register on kEventInitLua
Source/
  pdxinfo
  zzfx.lua        the zzfx({...}) / zzfxGetNote() wrapper
  main.lua        demo: play sounds on button presses + crank
```

## Demo controls

A = Coin · B = Laser · Up = Jump · Down = Explosion · Left = Hit ·
Right = Powerup · Crank = play a musical scale.

## Notes

- **Loudness.** ZzFX applies its 0.3 master volume twice (once while building,
  once on playback), which this port reproduces for an identical sound. If the
  tiny Playdate speaker is too quiet for you, raise `ZZFX_PLAYBACK_VOLUME` in
  `src/zzfx.c` (up to `1.0`).
- **SDK version.** Uses the current `newSampleFromData` signature with the
  `shouldFreeData` argument (Playdate SDK 2.x+). On an older SDK, drop the
  trailing `, 1` argument in `src/zzfx.c`.

MIT License — ZzFX © 2019 Frank Force.

## Preparing a GitHub release

This repo now includes release-friendly project metadata:

- `LICENSE` (MIT)
- `.gitignore` (ignores local/build outputs like `build/` and `ZzFX.pdx/`)
- `.gitattributes` (normalizes text files)
- `CHANGELOG.md`
- `CONTRIBUTING.md`
- `RELEASING.md` (maintainer release runbook)
- `.github/ISSUE_TEMPLATE/*` and `.github/pull_request_template.md`

### Suggested first publish flow

```bash
git init
git add .
git commit -m "chore: prepare initial public release"
git branch -M main
git remote add origin https://github.com/<your-user>/<your-repo>.git
git push -u origin main
```

### Tagging v1.0.0

```bash
git tag -a v1.0.0 -m "Initial public release"
git push origin v1.0.0
```

When creating the GitHub Release, use the `v1.0.0` tag and copy notes from `CHANGELOG.md`.

For ongoing releases after first publish, follow `RELEASING.md`.
