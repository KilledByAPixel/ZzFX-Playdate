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
    { key = "A",     name = "Coin",      params = {nil,nil,1675,nil,.06,.24,1,1.82,nil,nil,837,.06} },
    { key = "B",     name = "Laser",     params = {nil,nil,471,nil,.09,.47,4,1.06,-6.7,nil,nil,nil,nil,.9,nil,.6,nil,.62,.06} },
    { key = "Up",    name = "Jump",      params = {nil,nil,254,.02,nil,.02,nil,nil,7} },
    { key = "Down",  name = "Explosion", params = {nil,nil,782,.03,.09,.31,3,2.62,nil,nil,nil,nil,nil,1.5,nil,.6,.06,.58} },
    { key = "Left",  name = "Hit",       params = {nil,nil,925,.04,.3,.6,1,.3,nil,6.27,-184,.09,.17} },
    { key = "Right", name = "Powerup",   params = {nil,nil,1300,nil,nil,.2,1,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,.1} },
}

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
    for _, s in ipairs(sounds) do
        if s.key == key then
            zzfx(s.params)
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
    draw()
end

-- Button handlers
function playdate.AButtonDown()      playByKey("A")     end
function playdate.BButtonDown()      playByKey("B")     end
function playdate.upButtonDown()     playByKey("Up")    end
function playdate.downButtonDown()   playByKey("Down")  end
function playdate.leftButtonDown()   playByKey("Left")  end
function playdate.rightButtonDown()  playByKey("Right") end

-- Crank: play notes on a scale, one per ~30 degrees, using zzfxGetNote.
local lastNoteStep = nil
function playdate.cranked(change, acceleratedChange)
    local step = math_floor(playdate.getCrankPosition() / 30)
    if step ~= lastNoteStep then
        lastNoteStep = step
        -- a simple beep at the note frequency
        local freq = zzfxGetNote(step, 220)
        local params = {nil, nil, freq, nil, .04, .12, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, .05}
        zzfx(params)
        updateWaveform(params)
        lastPlayed = "note " .. step
    end
end

updateWaveform(nil)
