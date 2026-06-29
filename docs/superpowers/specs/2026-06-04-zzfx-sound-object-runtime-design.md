# ZzFX for Playdate — Sound Object Runtime (Design)

**Date:** 2026-06-04
**Status:** Approved design, pending implementation plan
**Scope:** Runtime sound library only. On-device (ARM) build and GitHub
repo/release polish are explicit follow-ups, **not** part of this design.

## 1. Goal

Evolve the current fire-and-forget `zzfx({...})` function into a LittleJS-parity
**`Sound` object model**: bake a ZzFX sound to a cached sample once, then play it
many times cheaply, applying per-play variation via **playback-rate jitter**
instead of re-synthesizing. Every `:play()` returns a controllable instance.

This mirrors the LittleJS `Sound`/`SoundInstance` classes so users' mental model
carries over from the web library.

### Why rate-jitter instead of regenerating

ZzFX's `randomness` parameter only perturbs the **starting frequency**. LittleJS
exploits this: it bakes the sample with randomness stripped (set to 0), stores
the randomness value, and on each play computes a randomized **playback rate**.
This is the established ZzFX approach. The trade-off — a rate change shifts pitch
*and* duration together, whereas baked-in randomness shifts pitch only — is
negligible for the small randomness values used in practice (~5%).

## 2. Public Lua API

```lua
import "zzfx"

-- Bake once: synthesizes immediately, caches the sample, strips randomness out.
local coin = Sound({nil,nil,1675,nil,.06,.24,1,1.82,nil,nil,837,.06})

-- Play (polyphonic by default). All args optional.
coin:play()                                   -- auto rate-randomness
coin:play(volume, pitch, randomnessScale, loop, pan)

-- Play as a musical note: pitch via rate-shift of the cached sample, no randomness.
coin:playNote(semitoneOffset, volume)

-- Returned instance: a live handle over the borrowed voice.
local inst = coin:play()
inst:stop()
inst:setVolume(v)         -- optionally (v, pan)
inst:setPaused(true)      -- or false to resume
inst:isPlaying()          -- bool

-- One-shot, unchanged for designer paste compatibility (returns an instance too).
zzfx({nil,nil,1675,nil,.06,.24,1,1.82,nil,nil,837,.06})

-- Paste helper: parse the designer's bracketed array string (empty slots -> nil).
zzfxString("[,,1675,,.06,.24,1,1.82,,,837,.06]")

-- Unchanged.
zzfxGetNote(semitoneOffset, rootNoteFrequency)
```

### Constructor — `Sound(zzfxArray, randomness, options)`

- `zzfxArray` — ZzFX parameter table (nil = default), same format as `zzfx()`.
- `randomness` (optional) — used **only if** the array's own randomness slot
  (index 2) is nil. Resolution order (LittleJS parity):
  `array[2]` → constructor `randomness` arg → `0.05`.
  The resolved value is **stored on the object**, then the baked copy's slot 2
  is **forced to 0** so randomness is applied on playback, not baked in.
- `options` (optional) — `{ mono = true }` for the monophonic opt-in.

### `Sound:play(volume, pitch, randomnessScale, loop, pan)`

Defaults: `volume=1`, `pitch=1`, `randomnessScale=1`, `loop=false`, `pan=0`.
Returns a `SoundInstance`. Polyphonic unless the Sound was created with
`{ mono = true }`.

### Rate math (heart of the per-play variation, LittleJS-exact)

```
rate = pitch + pitch * randomness * randomnessScale * rand(-1, 1)
```

where `randomness` is the value stored on the `Sound`, and `rand(-1,1)` is a
uniform random in [-1, 1].

### `Sound:playNote(semitoneOffset, volume)`

Plays the cached sample rate-shifted by `2^(semitoneOffset/12)`, randomness 0.
Pitch by **resampling**, not re-synthesis. Documented caveat: resampling shifts
pitch *and* duration and lightly alters timbre vs. re-synthesizing at that
frequency. Matches LittleJS; fine for short UI/scale beeps.

### `SoundInstance`

Thin wrapper over the borrowed sampleplayer: `stop`, `setVolume` (optionally with
pan), `setPaused`, `isPlaying`.

## 3. Internals (Approach 1: C bakes a sample, Lua owns the model)

### C side — `src/zzfx.c`

- **Keep `zzfx_build()` exactly as-is** — the verified, sample-accurate synth.
  No algorithm change. C no longer applies randomness at play time because Lua
  bakes with the randomness slot = 0.
- **Replace the Lua binding** with `__zzfxSample(p1..p21)`: builds the PCM, wraps
  it via `newSampleFromData(..., shouldFreeData=1)`, and returns it to Lua as a
  `playdate.sound.sample` using `pd->lua->pushObject(sample,
  "playdate.sound.sample", 0)`. The sample owns and frees its PCM buffer.
- **Remove the C-side voice pool** (`gVoices`, `zzfx_play`, round-robin cursor).
  Pooling moves to Lua. `zzfx_init` simplifies to storing `PD` and seeding
  `srand`.

### Lua side — `Source/zzfx.lua`

- **Shared voice pool:** module-level array of 16 `sampleplayer`s. `play` borrows
  a free one (none currently playing); if all busy, steal the oldest. A single
  sampleplayer swaps samples via `setSample`, so the pool is **generic across all
  Sounds** — 16 players total, not per-Sound.
- **`Sound`** holds: cached `sample`, stored `randomness`, `mono` flag (and, when
  mono, a dedicated player reference that is reused/restarted on retrigger).
- **`Sound:play(...)`:** acquire voice → `setSample` → `setRate(rate)` →
  `setVolume(left, right)` (from volume + pan) → `play(loop and 0 or 1)` →
  return a `SoundInstance` wrapping that player.
- **`SoundInstance`:** delegates `stop / setVolume / setPaused / isPlaying`
  straight to its sampleplayer.
- **`zzfx({...})`:** convenience — build a transient `Sound` and play once. No
  persistent caching (each call re-synthesizes). Returns the instance.
- **`zzfxString(str)`:** parse a bracketed ZzFX array string, converting empty
  fields to `nil`, then play (returns the instance). ~15 lines.
- **`zzfxGetNote`:** unchanged.

### Pan → per-channel volume

`pan` (−1..1) maps to `setVolume(left, right)`:

```
left  = volume * (1 - max(0, pan))
right = volume * (1 + min(0, pan))
```

Simple linear pan; `pan = 0` → `setVolume(volume, volume)`. No equal-power curve
unless desired later.

### Linchpin / first implementation step

`pushObject` handing a built-in `sample` to Lua is the one unproven assumption.
**Step 1 is a ~10-line spike:** bake a sample in C, return it, and from Lua call
`:getLength()` and play it.

- Pass → proceed with Approach 1.
- Fail → fall back to **Approach 2** (C owns synthesis, a sample cache, and the
  player pool, exposing integer handles: `bake(params)→id`,
  `play(id,rate,vol,pan,loop)→voice`, `stop(voice)`, etc.). The **public
  Section-2 API is identical either way** — only the Lua/C boundary differs.

## 4. Error handling

- C sample build failure → `__zzfxSample` returns nil; `Sound` construction
  raises a clear error.
- Pool exhaustion → steal the oldest playing voice (no crash, no silent drop).
- Invalid/missing params → fall back to ZzFX defaults (existing behavior).

## 5. Testing & verification

1. **C↔Lua spike (gating):** bake → return → Lua confirms a real sample
   (`:getLength() > 0`) that audibly plays. Validates Approach 1.
2. **Synth fidelity (regression):** `zzfx_build` is unchanged; re-run the
   existing Node sample-for-sample comparison against JS `zzfxG` after the
   refactor to confirm output didn't drift (target: max diff ~5e-16).
3. **Lua logic harness (sim console):**
   - `Sound(...)` bakes → `getDuration() > 0`.
   - Randomness stripped from baked copy but stored on the object.
   - `rate` math stays within `pitch ± pitch*randomness*scale`.
   - 17 rapid overlapping plays → 16 voices + oldest stolen, no crash.
   - `mono` Sound retriggered → reuses one voice.
   - `SoundInstance:isPlaying()` flips true→false across a stop.
   - `zzfxString("[,,1675,,.06]")` → `{nil,nil,1675,nil,.06}`.
4. **Manual ear check:** update `main.lua` demo — buttons play overlapping
   polyphonic `Sound`s, crank runs `playNote` up a scale, plus a mono example and
   a pan sweep. The "does it sound right" pass in the simulator.

## 6. Out of scope (follow-ups)

- On-device (ARM / `arm-none-eabi-gcc`) build verification.
- GitHub repo setup and release polish (README, LICENSE, .gitignore, examples).
- A "Playdate mode" export button in the ZzFX web designer (lives in the
  designer's repo, not here). Complements `zzfxString`.
