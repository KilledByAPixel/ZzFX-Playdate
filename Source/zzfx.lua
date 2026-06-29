--
-- zzfx.lua - Lua front end for the C ZzFX synth.
--
-- The C side (src/zzfx.c) registers a global __zzfx(...) that takes 21
-- positional numbers. This wrapper lets you call ZzFX exactly the way the
-- web library / sound designer expects: pass a single table of parameters.
--
-- IMPORTANT: JavaScript sparse arrays like  zzfx(...[,,1675,,.06])  are NOT
-- valid Lua. When you paste a sound from the ZzFX designer, replace every
-- empty slot (the gaps between commas, and leading commas) with `nil`:
--
--     JS :  zzfx(...[,,1675,,.06,.24,1,1.82,,,837,.06])
--     Lua:  zzfx({nil,nil,1675,nil,.06,.24,1,1.82,nil,nil,837,.06})
--
-- Any trailing parameters you don't need can simply be left off.
--

-- ZzFX parameter defaults (same order as ZZFX.buildSamples):
-- volume, randomness, frequency, attack, sustain, release, shape, shapeCurve,
-- slide, deltaSlide, pitchJump, pitchJumpTime, repeatTime, noise, modulation,
-- bitCrush, delay, sustainVolume, decay, tremolo, filter
local DEFAULTS <const> = {
    1, .05, 220, 0, 0, .1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0
}

-- Play a ZzFX sound. `params` is a table in the ZzFX array format (nil = use
-- default). Returns nothing.
function zzfx(params)
    params = params or {}
    -- Build a dense 21-element argument list (Lua treats 0 as truthy, so the
    -- `or` only substitutes a default for nil values, never for a real 0).
    local a = {}
    for i = 1, 21 do
        a[i] = params[i]
        if a[i] == nil then a[i] = DEFAULTS[i] end
    end
    return __zzfx(
        a[1],  a[2],  a[3],  a[4],  a[5],  a[6],  a[7],
        a[8],  a[9],  a[10], a[11], a[12], a[13], a[14],
        a[15], a[16], a[17], a[18], a[19], a[20], a[21])
end

-- Frequency of a note on the standard diatonic scale (ZZFX.getNote).
-- e.g. zzfxGetNote(0) == 440 (A4); zzfxGetNote(12) == 880.
function zzfxGetNote(semitoneOffset, rootNoteFrequency)
    semitoneOffset = semitoneOffset or 0
    rootNoteFrequency = rootNoteFrequency or 440
    return rootNoteFrequency * 2 ^ (semitoneOffset / 12)
end
