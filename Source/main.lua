--
-- ZzFX for Playdate - demo
--
-- Press the buttons / turn the crank to hear ZzFX sounds synthesized live on
-- the Playdate. No audio files are used: every sound is generated from a small
-- parameter array, exactly like the web ZzFX library.
--
import "CoreLibs/graphics"
import "zzfx"   -- provides the global zzfx({...})

local gfx <const> = playdate.graphics
local math_floor <const> = math.floor
local math_min <const> = math.min
local math_max <const> = math.max
local math_sin <const> = math.sin
local math_pi <const> = math.pi
local math_abs <const> = math.abs

-- A handful of ZzFX presets. Paste your own from https://killedbyapixel.github.io/ZzFX/
-- (remember: replace JS empty array slots with `nil`).
local sounds <const> = {
    { key = "A",     name = "Coin",      params = 
    {nil,nil,1675,nil,.06,.24,1,nil,nil,nil,837,.06} },
    { key = "B",     name = "Shoot",     params = {nil,nil,750,nil,nil,.3,1,nil,-3,nil,nil,nil,nil,nil,.6} },
    { key = "Up",    name = "Jump",      params = {1.1,nil,250,.02,nil,.07,nil,nil,9} },
    { key = "Down",  name = "Explosion", params = {1.1,nil,800,.03,.1,.3,nil,nil,nil,nil,nil,nil,nil,1.5,nil,.6,nil,.6} },
    { key = "Left",  name = "Hit",       params = {1.2,nil,320,.01,.07,.06,1,nil,-3,9,nil,nil,nil,1,nil,.1,nil,.6,.04} },
    { key = "Right", name = "Powerup",   params = {nil,nil,700,nil,.04,.3,1,nil,nil,nil,370,.06,.09}},
}

-- The crank plays a musical scale. Build the beep ONCE as a cached sound, then
-- pitch each note by changing playback rate (sound:playNote) instead of
-- re-synthesizing a fresh ~0.8s sound on every crank step -- live synthesis per
-- step stalls noticeably on the actual device.
local crankBaseParams <const> = {nil,0,nil,.002,.1,.3,nil,nil,nil,nil,nil,nil,.2,nil,nil,nil,nil,.7,.1,.1}
local crankSound   -- built incrementally by buildNext(), with a progress display

-- Synthesizing the cached sounds takes a moment on device (each sound's synth
-- runs once, up front). We build ONE per frame and show "N of M" progress, so
-- the wait is visible instead of a black screen. `booted` gates input until the
-- sounds exist.
local booted = false
local buildIndex = 0                 -- how many sounds built so far
local buildTotal = #sounds + 1       -- the button presets, plus the crank sound

-- Build the next not-yet-built sound; flips `booted` once they're all done.
local function buildNext()
    buildIndex = buildIndex + 1
    if buildIndex <= #sounds then
        sounds[buildIndex].sound = zzfxSound(sounds[buildIndex].params)
    else
        crankSound = zzfxSound(crankBaseParams)
    end
    if buildIndex >= buildTotal then booted = true end
end

local zzfxDefaults <const> = {
    1, .05, 220, 0, 0, .1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0
}

local lastPlayed = "press a button"
local waveformPoints = {}

local function clamp(value, lo, hi)
    return math_max(lo, math_min(hi, value))
end

local function expandParams(params)
    local dense = {}
    for i = 1, 21 do
        local v = params and params[i] or nil
        if v == nil then
            v = zzfxDefaults[i]
        end
        dense[i] = v
    end
    return dense
end

local function shapeSample(shape, phase, seed)
    local phaseNorm = (phase / (2 * math_pi)) % 1
    if shape >= 3.5 then
        -- Deterministic pseudo-noise for a stable preview.
        local n = math_sin(seed * 12.9898 + phase * 78.233) * 43758.5453
        return ((n % 1) * 2) - 1
    elseif shape >= 2.5 then
        return math_sin(phase) >= 0 and 1 or -1
    elseif shape >= 1.5 then
        return (2 * phaseNorm) - 1
    elseif shape >= 0.5 then
        return (2 / math_pi) * math.asin(math_sin(phase))
    end
    return math_sin(phase)
end

local function updateWaveform(params)
    local p = expandParams(params)
    local pointCount = 120
    local attack = math_max(0, p[4])
    local sustain = math_max(0, p[5])
    local release = math_max(0.01, p[6])
    local totalTime = math_max(0.08, attack + sustain + release)
    local baseFreq = clamp(p[3], 20, 5000)
    local slide = p[9]
    local shape = p[7]
    local phase = 0
    local peak = 0.0001

    waveformPoints = {}
    for i = 1, pointCount do
        local t = ((i - 1) / (pointCount - 1)) * totalTime
        local env
        if t < attack and attack > 0 then
            env = t / attack
        elseif t < attack + sustain then
            env = 1
        else
            local r = (t - attack - sustain) / release
            env = clamp(1 - r, 0, 1)
        end

        local freq = clamp(baseFreq + slide * t * 100, 20, 8000)
        phase = phase + (2 * math_pi * freq / 44100) * 220
        local sample = env * shapeSample(shape, phase, i)
        waveformPoints[i] = sample
        local mag = math_abs(sample)
        if mag > peak then
            peak = mag
        end
    end

    for i = 1, #waveformPoints do
        waveformPoints[i] = waveformPoints[i] / peak
    end
end

local function playByKey(key)
    if not booted then return end
    for _, s in ipairs(sounds) do
        if s.key == key then
            s.sound:play()
            lastPlayed = s.name
            updateWaveform(s.params)
            return
        end
    end
end

local function drawWaveformPanel()
    local panelX, panelY, panelW, panelH = 228, 54, 160, 126
    gfx.drawRect(panelX, panelY, panelW, panelH)

    local midY = panelY + math_floor(panelH / 2)
    gfx.drawLine(panelX + 6, midY, panelX + panelW - 6, midY)

    if #waveformPoints < 2 then
        gfx.drawText("press a control", panelX + 24, panelY + 52)
        return
    end

    local innerX = panelX + 6
    local innerY = panelY + 8
    local innerW = panelW - 12
    local innerH = panelH - 16
    local prevX = innerX
    local prevY = innerY + math_floor((1 - (waveformPoints[1] + 1) * 0.5) * innerH)

    for i = 2, #waveformPoints do
        local x = innerX + math_floor(((i - 1) / (#waveformPoints - 1)) * innerW)
        local y = innerY + math_floor((1 - (waveformPoints[i] + 1) * 0.5) * innerH)
        gfx.drawLine(prevX, prevY, x, y)
        prevX = x
        prevY = y
    end
end

local function draw()
    gfx.clear()
    gfx.drawText("*ZzFX for Playdate*", 12, 10)
    gfx.drawText("Synthesized live - no sound files", 12, 30)

    local y = 58
    local lineStep = 18
    for _, s in ipairs(sounds) do
        gfx.drawText(s.key .. "  -  " .. s.name, 40, y)
        y = y + lineStep
    end

    gfx.drawText("Crank  -  Musical scale", 40, y)
    gfx.drawText("Last  -  " .. lastPlayed, 12, 206)

    drawWaveformPanel()
end

function playdate.update()
    if not booted then
        -- Build one sound per frame, drawing progress between each. (The frame is
        -- shown after update() returns, so each "N of M" appears as that sound
        -- finishes.) On the Simulator this flies by; on device you'll see it count.
        gfx.clear()
        gfx.drawTextAligned("*Loading sounds " .. (buildIndex + 1) .. " of " .. buildTotal .. "*",
            200, 112, kTextAlignment.center)
        buildNext()
        return
    end
    draw()
end

-- Button handlers
function playdate.AButtonDown()      playByKey("A")     end
function playdate.BButtonDown()      playByKey("B")     end
function playdate.upButtonDown()     playByKey("Up")    end
function playdate.downButtonDown()   playByKey("Down")  end
function playdate.leftButtonDown()   playByKey("Left")  end
function playdate.rightButtonDown()  playByKey("Right") end

-- Crank plays an ascending C major scale -- one octave (do re mi fa sol la ti do)
-- per full rotation, pitched via the cached sound's playback rate.
local majorScale <const> = { 0, 2, 4, 5, 7, 9, 11, 12 }   -- semitone offsets
local lastNoteStep = nil
function playdate.cranked(change, acceleratedChange)
    if not booted then return end
    local step = math_floor(playdate.getCrankPosition() / (360 / #majorScale)) % #majorScale
    if step ~= lastNoteStep then
        lastNoteStep = step
        crankSound:playNote(majorScale[step + 1])   -- pitch the cached sound; instant
        updateWaveform(crankBaseParams)
        lastPlayed = "note " .. (step + 1)
    end
end

updateWaveform(nil)
