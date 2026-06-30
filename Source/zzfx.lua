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
    return __zzfx(table.unpack(denseParams(params), 1, 21))
end

-- Shared sound object used by both the synthesized and WAV-backed paths, so
-- they expose the exact same interface. `playAtRate(rate)` triggers the sound;
-- `freeFn()` releases it. Randomness is applied HERE as a per-play playback-rate
-- (pitch) wobble -- which is why a baked WAV behaves just like a synth sound.
local function makeSound(randomness, playAtRate, freeFn)
    local sound = { randomness = randomness or 0 }

    function sound:play(pitch, randomnessScale)
        pitch = pitch or 1
        randomnessScale = randomnessScale or 1
        local rate = pitch + pitch * self.randomness * randomnessScale * (math.random() * 2 - 1)
        if rate < 0.01 then rate = 0.01 end
        playAtRate(rate)
    end

    function sound:playNote(semitoneOffset)
        self:play(2 ^ ((semitoneOffset or 0) / 12), 0)
    end

    function sound:free()
        if freeFn then freeFn() end
    end

    return sound
end

-- Build a reusable ZzFX sound object. `source` is either:
--   * a ZzFX params table   -> synthesized once and cached (the usual path), or
--   * a WAV/sample filename  -> loaded and wrapped in the SAME interface, so you
--     can bake sounds to WAV for a faster-loading release and swap them in with
--     no other code changes.
-- Optional `randomness` is applied as per-play pitch variation. For the WAV case,
-- export the file with randomness 0 and pass it here, matching how the
-- synthesized path varies pitch at play time.
function zzfxSound(source, randomness)
    if type(source) == "string" then
        local player = playdate.sound.sampleplayer.new(source)
        assert(player, "zzfxSound: could not load sample '" .. source .. "'")
        return makeSound(randomness or 0,
            function(rate) player:setRate(rate); player:play() end,
            function() player:stop() end)
    end

    -- params table: synthesize and cache (randomness off in the buffer; the
    -- variation is applied per play instead).
    local a = denseParams(source)
    local soundRandomness = randomness or a[2]
    a[2] = 0

    local id = __zzfxCacheNew(table.unpack(a, 1, 21))
    assert(id and id ~= 0, "zzfxSound cache allocation failed; create fewer cached sounds or increase ZZFX_MAX_CACHED")

    return makeSound(soundRandomness,
        function(rate) __zzfxCachePlay(id, rate) end,
        function() if id then __zzfxCacheFree(id); id = nil end end)
end

-- Frequency of a note on the standard diatonic scale (ZZFX.getNote).
-- e.g. zzfxGetNote(0) == 440 (A4); zzfxGetNote(12) == 880.
function zzfxGetNote(semitoneOffset, rootNoteFrequency)
    semitoneOffset = semitoneOffset or 0
    rootNoteFrequency = rootNoteFrequency or 440
    return rootNoteFrequency * 2 ^ (semitoneOffset / 12)
end
