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

local function denseParams(params)
    params = params or {}
    local a = {}
    for i = 1, 21 do
        local v = params[i]
        if v == nil then v = DEFAULTS[i] end
        a[i] = v
    end
    return a
end

-- Play a ZzFX sound. `params` is a table in the ZzFX array format (nil = use
-- default). Returns nothing.
function zzfx(params)
    local a = denseParams(params)
    return __zzfx(
        a[1],  a[2],  a[3],  a[4],  a[5],  a[6],  a[7],
        a[8],  a[9],  a[10], a[11], a[12], a[13], a[14],
        a[15], a[16], a[17], a[18], a[19], a[20], a[21])
end

-- Build a cached ZzFX sound object and play it repeatedly with optional
-- per-play randomness (applied as playback-rate variation).
function zzfxSound(params, randomness)
    local a = denseParams(params)
    local soundRandomness = randomness
    if soundRandomness == nil then
        soundRandomness = a[2]
    end

    -- Build and cache with fixed randomness; apply variation at play time.
    a[2] = 0

    local id = __zzfxCacheNew(
        a[1],  a[2],  a[3],  a[4],  a[5],  a[6],  a[7],
        a[8],  a[9],  a[10], a[11], a[12], a[13], a[14],
        a[15], a[16], a[17], a[18], a[19], a[20], a[21])

    assert(id and id ~= 0, "zzfxSound cache allocation failed; create fewer cached sounds or increase ZZFX_MAX_CACHED")

    local sound = {
        id = id,
        params = a,
        randomness = soundRandomness,
    }

    function sound:play(pitch, randomnessScale)
        pitch = pitch or 1
        randomnessScale = randomnessScale or 1
        local rate = pitch + pitch * self.randomness * randomnessScale * (math.random() * 2 - 1)
        if rate < 0.01 then rate = 0.01 end
        __zzfxCachePlay(self.id, rate)
    end

    function sound:playNote(semitoneOffset)
        semitoneOffset = semitoneOffset or 0
        local pitch = 2 ^ (semitoneOffset / 12)
        self:play(pitch, 0)
    end

    function sound:free()
        if self.id then
            __zzfxCacheFree(self.id)
            self.id = nil
        end
    end

    return sound
end

-- Frequency of a note on the standard diatonic scale (ZZFX.getNote).
-- e.g. zzfxGetNote(0) == 440 (A4); zzfxGetNote(12) == 880.
function zzfxGetNote(semitoneOffset, rootNoteFrequency)
    semitoneOffset = semitoneOffset or 0
    rootNoteFrequency = rootNoteFrequency or 440
    return rootNoteFrequency * 2 ^ (semitoneOffset / 12)
end
