# ZzFX for Playdate

Tiny real-time sound effects for Playdate, powered by ZzFX.

Design a sound in the [ZzFX sound designer](https://killedbyapixel.github.io/ZzFX/),
paste the params into Lua, and play it instantly. No audio asset files needed.

This repo includes a playable demo app.

## Make sounds with the ZzFX editor

Use the ZzFX sound editor here:

https://killedbyapixel.github.io/ZzFX/

Fast workflow:

1. Build a sound in the editor.
2. Copy the generated ZzFX array.
3. Replace JavaScript empty slots with `nil`.
4. Paste into `zzfx({...})` in Lua.

## Quick start

```lua
import "zzfx"

-- Coin
zzfx({nil, nil, 1675, nil, .06, .24, 1, 1.82, nil, nil, 837, .06})

-- Simple beep
zzfx({nil, nil, 220})

-- Note helper
local f = zzfxGetNote(7, 220)
zzfx({nil, nil, f, nil, .04, .12})
```

## Pasting from the ZzFX designer

The designer outputs JavaScript sparse arrays, for example:

```js
zzfx(...[,,1675,,.06,.24,1,1.82,,,837,.06]);
```

In Lua, replace each gap with `nil`:

```lua
zzfx({nil,nil,1675,nil,.06,.24,1,1.82,nil,nil,837,.06})
```

Trailing unused params can be omitted.

## Build

Requires the [Playdate SDK](https://play.date/dev/) and `PLAYDATE_SDK_PATH` set.

```bash
make            # Simulator build -> ZzFX.pdx
make device     # Device build (needs arm-none-eabi-gcc)
```

Open `ZzFX.pdx` in Playdate Simulator.

### Windows (CMake)

```powershell
$env:PLAYDATE_SDK_PATH = "C:\Program Files (x86)\Playdate"
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Then open `ZzFX.pdx` in the Playdate Simulator.

## What ships in the repo

- Demo source is shipped: `Source/main.lua` and `Source/zzfx.lua`.
- C synth source is shipped: `src/zzfx.c`, `src/zzfx.h`, and `src/main.c`.
- Built outputs are not shipped (`ZzFX.pdx/`, `build/`, `Source/pdex.dll` are ignored).

## Why both Source and src?

- `Source/` is the Playdate Lua/assets folder used by the SDK and simulator.
- `src/` contains C source for the native synth and Lua binding.

This is normal for mixed Lua + C Playdate projects.

## Demo controls

A = Coin, B = Laser, Up = Jump, Down = Explosion, Left = Hit,
Right = Powerup, Crank = musical scale.

## Demo screenshot

![ZzFX for Playdate demo screenshot](screenshot.png)

## API

- `zzfx(paramsTable)` plays one sound from ZzFX-style params (21-slot array format).
- `zzfxGetNote(semitoneOffset, rootFrequency)` returns note frequency.
- C API is available in `src/zzfx.h` (`zzfx_init`, `zzfx_register_lua`, `zzfx_play`).

## Notes

- This is a faithful C port of `ZZFX.buildSamples` at 44100 Hz.
- If output is too quiet on speaker, raise `ZZFX_PLAYBACK_VOLUME` in `src/zzfx.c`.

## License

MIT. See `LICENSE`.
