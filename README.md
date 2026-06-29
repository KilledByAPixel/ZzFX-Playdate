# ZzFX for Playdate

Tiny real-time sound effects for Playdate, powered by ZzFX.

Design a sound in the [ZzFX sound designer](https://killedbyapixel.github.io/ZzFX/),
paste the params into Lua, and play it instantly. No audio asset files needed.

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

## Demo controls

A = Coin, B = Laser, Up = Jump, Down = Explosion, Left = Hit,
Right = Powerup, Crank = musical scale.

## API

- `zzfx(paramsTable)` plays one sound from ZzFX-style params (21-slot array format).
- `zzfxGetNote(semitoneOffset, rootFrequency)` returns note frequency.
- C API is available in `src/zzfx.h` (`zzfx_init`, `zzfx_register_lua`, `zzfx_play`).

## Notes

- This is a faithful C port of `ZZFX.buildSamples` at 44100 Hz.
- If output is too quiet on speaker, raise `ZZFX_PLAYBACK_VOLUME` in `src/zzfx.c`.

## License

MIT. See `LICENSE`.
