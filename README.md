# ZzFX for Playdate

Tiny real-time sound effects for Playdate, powered by ZzFX.

Design a sound in the [ZzFX sound designer](https://killedbyapixel.github.io/ZzFX/),
paste the params into Lua, and play it instantly. No audio asset files needed.

This repo includes a playable demo app.

## Make sounds with the ZzFX editor

Open the ZzFX sound editor:

https://killedbyapixel.github.io/ZzFX/

Quick workflow:

1. Build a sound in the editor.
2. Select the Lua radio button.
3. Copy the generated Lua-style output.
4. Paste it directly into your Lua code.

## Quick start

```lua
import "zzfx"

-- Coin
zzfx({nil,0,988,nil,nil,.4,nil,33,nil,nil,331,.1})

-- Simple beep
zzfx({nil, nil, 220})

-- Note helper
local f = zzfxGetNote(7, 220)
zzfx({nil, nil, f, nil, .04, .12})
```

The Lua output is ready to paste as-is. No sparse-array conversion needed.

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

## API

- `zzfx(paramsTable)` plays one sound from ZzFX-style params (21-slot array format).
- `zzfxGetNote(semitoneOffset, rootFrequency)` returns note frequency.
- C API is available in `src/zzfx.h` (`zzfx_init`, `zzfx_register_lua`, `zzfx_play`).

## Demo

A = Coin, B = Laser, Up = Jump, Down = Explosion, Left = Hit,
Right = Powerup, Crank = musical scale.

![ZzFX for Playdate demo screenshot](screenshot.png)
