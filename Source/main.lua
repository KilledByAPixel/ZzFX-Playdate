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

local lastPlayed = "press a button"

local function playByKey(key)
    for _, s in ipairs(sounds) do
        if s.key == key then
            zzfx(s.params)
            lastPlayed = s.name
            return
        end
    end
end

local function draw()
    gfx.clear()
    gfx.drawText("*ZzFX for Playdate*", 12, 10)
    gfx.drawText("Synthesized live - no sound files", 12, 30)

    local y = 64
    for _, s in ipairs(sounds) do
        gfx.drawText(s.key .. "  -  " .. s.name, 40, y)
        y = y + 22
    end

    gfx.drawText("crank: musical scale", 40, y + 6)
    gfx.drawText("Last: " .. lastPlayed, 12, 214)
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
    local step = math.floor(playdate.getCrankPosition() / 30)
    if step ~= lastNoteStep then
        lastNoteStep = step
        -- a simple beep at the note frequency
        local freq = zzfxGetNote(step, 220)
        zzfx({nil, nil, freq, nil, .04, .12, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, nil, .05})
        lastPlayed = "note " .. step
    end
end
