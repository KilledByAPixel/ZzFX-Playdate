//
//  zzfx.c - ZzFX sound effect synth, ported to the Playdate C API.
//  See zzfx.h. MIT License (c) Frank Force.
//
#include "zzfx.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---- configuration ---------------------------------------------------------

#define ZZFX_SAMPLE_RATE   44100     // Playdate's native audio rate
#define ZZFX_NUM_VOICES    16        // how many sounds can overlap
#define ZZFX_MAX_CACHED    64        // max cached zzfxSound objects

// ZZFX.volume from the JS library (master scale). NOTE: the original applies
// this twice (once while building samples, once on the playback gain node),
// which gives ZzFX its characteristic, slightly conservative level. We keep
// both for an identical sound. Raise ZZFX_PLAYBACK_VOLUME if you want the
// Playdate speaker to be louder.
#define ZZFX_BUILD_VOLUME     0.3    // applied inside buildSamples (do not change for fidelity)
#define ZZFX_PLAYBACK_VOLUME  0.3    // applied on playback (bump toward 1.0 to get louder)

// ---- state -----------------------------------------------------------------

static PlaydateAPI* PD = NULL;

typedef struct {
    SamplePlayer* player;
    AudioSample*  sample;   // freed (with its data) before the slot is reused
    int ownsSample;         // 1 for one-shot samples, 0 for cached/shared samples
} ZzfxVoice;

static ZzfxVoice gVoices[ZZFX_NUM_VOICES];
static int gVoiceCursor = 0;

typedef struct {
    AudioSample* sample;
    int used;
} ZzfxCachedSound;

static ZzfxCachedSound gCached[ZZFX_MAX_CACHED];

// ---- helpers ----------------------------------------------------------------

static double zzfx_rand(void)            { return (double)rand() / ((double)RAND_MAX + 1.0); }
static double sgn(double v)              { return v < 0.0 ? -1.0 : 1.0; }

// ---- the synth (faithful port of ZZFX.buildSamples) -------------------------
//
// Builds a 16-bit mono PCM buffer. Returns a buffer allocated with
// pd->system->realloc (so the sample object can own and later free it), and
// writes the sample count to *outLen. Returns NULL on failure.
static int16_t* zzfx_build(
    double volume,        double randomness,    double frequency,    double attack,
    double sustain,       double release,       double shape,        double shapeCurve,
    double slide,         double deltaSlide,    double pitchJump,    double pitchJumpTime,
    double repeatTime,    double noise,         double modulation,   double bitCrush,
    double delay,         double sustainVolume, double decay,        double tremolo,
    double filter,        int* outLen)
{
    const double PI2 = M_PI * 2.0;
    const double sampleRate = (double)ZZFX_SAMPLE_RATE;

    // init parameters (mirrors the JS, line for line)
    double startSlide     = slide *= 500.0 * PI2 / sampleRate / sampleRate;
    double startFrequency = frequency *=
        (1.0 + randomness * 2.0 * zzfx_rand() - randomness) * PI2 / sampleRate;
    double modOffset = 0.0;
    long   repeat = 0, crush = 0;
    long   jump = 1;
    double t = 0.0, s = 0.0, f;

    // biquad LP/HP filter coefficients
    double quality = 2.0, w = PI2 * fabs(filter) * 2.0 / sampleRate;
    double cosw = cos(w), alpha = sin(w) / 2.0 / quality;
    double a0 = 1.0 + alpha, a1 = -2.0 * cosw / a0, a2 = (1.0 - alpha) / a0;
    double b0 = (1.0 + sgn(filter) * cosw) / 2.0 / a0;
    double b1 = -(sgn(filter) + cosw) / a0, b2 = b0;
    double x2 = 0, x1 = 0, y2 = 0, y1 = 0;

    // scale by sample rate
    const double minAttack = 9.0;          // prevent pop if attack is 0
    attack = attack * sampleRate;
    if (attack == 0.0) attack = minAttack;
    decay   *= sampleRate;
    sustain *= sampleRate;
    release *= sampleRate;
    delay   *= sampleRate;
    deltaSlide   *= 500.0 * PI2 / (sampleRate * sampleRate * sampleRate);
    modulation   *= PI2 / sampleRate;
    pitchJump    *= PI2 / sampleRate;
    pitchJumpTime *= sampleRate;
    long repeatTimeI = (long)(repeatTime * sampleRate);   // | 0
    volume *= ZZFX_BUILD_VOLUME;

    int sh = (int)shape;
    int crushStep = (int)(bitCrush * 100.0);              // bitCrush*100 | 0

    int length = (int)(attack + decay + sustain + release + delay);  // | 0
    if (length < 1) length = 1;

    // float work buffer (matches JS b[]); used for delay feedback too
    double* b = (double*)PD->system->realloc(NULL, sizeof(double) * (size_t)length);
    if (!b) { *outLen = 0; return NULL; }

    for (int i = 0; i < length; i++)
    {
        // bit crush gate. In JS, (bitCrush*100|0)==0 makes (x % 0)==NaN, so
        // !(NaN) is true and the sample is recomputed every step.
        crush++;
        int doSample = (crushStep == 0) || ((crush % crushStep) == 0);

        if (doSample)
        {
            // wave shape
            if      (sh == 0) s = sin(t);                                          // sin
            else if (sh == 1) s = 1.0 - 4.0 * fabs(floor(t / PI2 + 0.5) - t / PI2);// triangle
            else if (sh == 2) s = 1.0 - fmod(fmod(2.0 * t / PI2, 2.0) + 2.0, 2.0); // saw
            else if (sh == 3) { double tv = tan(t); s = tv > 1.0 ? 1.0 : (tv < -1.0 ? -1.0 : tv); } // tan
            else if (sh == 4) s = sin(t * t * t);                                  // noise
            else              s = (fmod(t / PI2, 1.0) < shapeCurve / 2.0) ? 1.0 : -1.0; // square duty

            double trem = repeatTimeI
                ? (1.0 - tremolo + tremolo * sin(PI2 * (double)i / (double)repeatTimeI))
                : 1.0;

            double shaped = (sh > 4) ? s : sgn(s) * pow(fabs(s), shapeCurve);

            double env;
            if      (i < attack)                       env = (double)i / attack;
            else if (i < attack + decay)               env = 1.0 - ((double)i - attack) / decay * (1.0 - sustainVolume);
            else if (i < attack + decay + sustain)     env = sustainVolume;
            else if (i < length - delay)               env = ((double)length - (double)i - delay) / release * sustainVolume;
            else                                       env = 0.0;

            s = trem * shaped * env;

            // delay
            if (delay != 0.0)
            {
                double dterm;
                if (delay > (double)i) dterm = 0.0;
                else {
                    double rel = ((double)i < (double)length - delay) ? 1.0 : ((double)length - (double)i) / delay;
                    dterm = rel * b[(long)((double)i - delay)] / 2.0 / volume;
                }
                s = s / 2.0 + dterm;
            }

            // biquad filter (assignments evaluated left-to-right as in JS)
            if (filter != 0.0)
            {
                double X2 = x2, X1 = x1, Y2 = y2, Y1 = y1;
                double out = b2 * X2 + b1 * X1 + b0 * s - a2 * Y2 - a1 * Y1;
                x2 = X1;   // x2 = x1
                x1 = s;    // x1 = s
                y2 = Y1;   // y2 = y1
                y1 = out;  // y1 = result
                s = out;
            }
        }

        b[i] = s * volume;   // store sample (JS: b[i++] = s * volume)

        // advance oscillator
        slide += deltaSlide;
        frequency += slide;
        f = frequency * cos(modulation * modOffset);
        modOffset += 1.0;
        t += f + f * noise * sin(pow((double)i, 5.0));

        // pitch jump
        if (jump && (++jump > pitchJumpTime))
        {
            frequency      += pitchJump;
            startFrequency += pitchJump;
            jump = 0;
        }

        // repeat
        if (repeatTimeI && ((++repeat % repeatTimeI) == 0))
        {
            frequency = startFrequency;
            slide     = startSlide;
            if (!jump) jump = 1;
        }
    }

    // convert to 16-bit PCM, applying the playback master gain and clamping
    int16_t* pcm = (int16_t*)PD->system->realloc(NULL, sizeof(int16_t) * (size_t)length);
    if (!pcm) { PD->system->realloc(b, 0); *outLen = 0; return NULL; }

    for (int i = 0; i < length; i++)
    {
        double v = b[i] * ZZFX_PLAYBACK_VOLUME;
        if (v != v) v = 0.0;                 // NaN guard
        if (v >  1.0) v =  1.0;
        if (v < -1.0) v = -1.0;
        pcm[i] = (int16_t)(v * 32767.0);
    }

    PD->system->realloc(b, 0);
    *outLen = length;
    return pcm;
}

// ---- public API -------------------------------------------------------------

void zzfx_init(PlaydateAPI* pd)
{
    PD = pd;
    srand((unsigned int)pd->system->getCurrentTimeMilliseconds());
    for (int i = 0; i < ZZFX_NUM_VOICES; i++)
    {
        gVoices[i].player = pd->sound->sampleplayer->newPlayer();
        gVoices[i].sample = NULL;
        gVoices[i].ownsSample = 0;
    }

    for (int i = 0; i < ZZFX_MAX_CACHED; i++)
    {
        gCached[i].sample = NULL;
        gCached[i].used = 0;
    }

    gVoiceCursor = 0;
}

void zzfx_play(const double p[21])
{
    if (!PD) return;

    int len = 0;
    int16_t* pcm = zzfx_build(p[0], p[1], p[2], p[3], p[4], p[5], p[6],
                              p[7], p[8], p[9], p[10], p[11], p[12], p[13],
                              p[14], p[15], p[16], p[17], p[18], p[19], p[20], &len);
    if (!pcm || len <= 0) { if (pcm) PD->system->realloc(pcm, 0); return; }

    ZzfxVoice* v = &gVoices[gVoiceCursor];
    gVoiceCursor = (gVoiceCursor + 1) % ZZFX_NUM_VOICES;

    // recycle the slot: stop and free whatever played here last
    PD->sound->sampleplayer->stop(v->player);
    if (v->sample && v->ownsSample)
        PD->sound->sample->freeSample(v->sample);
    v->sample = NULL;
    v->ownsSample = 0;

    // newSampleFromData keeps a pointer to (does not copy) the buffer.
    // shouldFreeData = 1 -> freeSample() frees the PCM buffer for us.
    v->sample = PD->sound->sample->newSampleFromData(
        (uint8_t*)pcm, kSound16bitMono, ZZFX_SAMPLE_RATE,
        len * (int)sizeof(int16_t), 1);

    if (!v->sample) { PD->system->realloc(pcm, 0); return; }

    PD->sound->sampleplayer->setSample(v->player, v->sample);
    PD->sound->sampleplayer->play(v->player, 1, 1.0f);
    v->ownsSample = 1;
}

// ---- Lua binding ------------------------------------------------------------
//
// Registered as the global __zzfx(...). The Lua wrapper in zzfx.lua unpacks a
// ZzFX sound table into 21 positional arguments (filling defaults for nils),
// so by the time we get here every argument should be a number; we still
// default-fill defensively in case __zzfx is called directly.
static int lua_zzfx(lua_State* L)
{
    (void)L;
    static const double defaults[21] = {
        1, 0.05, 220, 0, 0, 0.1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0
    };
    double p[21];
    int n = PD->lua->getArgCount();
    for (int i = 0; i < 21; i++)
    {
        int pos = i + 1;
        if (pos > n || PD->lua->argIsNil(pos)) p[i] = defaults[i];
        else                                   p[i] = (double)PD->lua->getArgFloat(pos);
    }
    zzfx_play(p);
    return 0;
}

static int lua_zzfx_cache_new(lua_State* L)
{
    (void)L;
    static const double defaults[21] = {
        1, 0.05, 220, 0, 0, 0.1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0
    };

    double p[21];
    int n = PD->lua->getArgCount();
    for (int i = 0; i < 21; i++)
    {
        int pos = i + 1;
        if (pos > n || PD->lua->argIsNil(pos)) p[i] = defaults[i];
        else                                   p[i] = (double)PD->lua->getArgFloat(pos);
    }

    int slot = -1;
    for (int i = 0; i < ZZFX_MAX_CACHED; i++)
    {
        if (!gCached[i].used)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        PD->system->logToConsole("zzfx: cache full (%d)", ZZFX_MAX_CACHED);
        PD->lua->pushInt(0);
        return 1;
    }

    int len = 0;
    int16_t* pcm = zzfx_build(p[0], p[1], p[2], p[3], p[4], p[5], p[6],
                              p[7], p[8], p[9], p[10], p[11], p[12], p[13],
                              p[14], p[15], p[16], p[17], p[18], p[19], p[20], &len);
    if (!pcm || len <= 0)
    {
        if (pcm) PD->system->realloc(pcm, 0);
        PD->lua->pushInt(0);
        return 1;
    }

    AudioSample* sample = PD->sound->sample->newSampleFromData(
        (uint8_t*)pcm, kSound16bitMono, ZZFX_SAMPLE_RATE,
        len * (int)sizeof(int16_t), 1);

    if (!sample)
    {
        PD->system->realloc(pcm, 0);
        PD->lua->pushInt(0);
        return 1;
    }

    gCached[slot].sample = sample;
    gCached[slot].used = 1;

    PD->lua->pushInt(slot + 1); // 1-based IDs for Lua
    return 1;
}

static int lua_zzfx_cache_play(lua_State* L)
{
    (void)L;
    int n = PD->lua->getArgCount();
    if (n < 1)
        return 0;

    int id = (int)PD->lua->getArgFloat(1);
    if (id < 1 || id > ZZFX_MAX_CACHED)
        return 0;

    int slot = id - 1;
    if (!gCached[slot].used || !gCached[slot].sample)
        return 0;

    float rate = 1.0f;
    if (n >= 2 && !PD->lua->argIsNil(2))
        rate = (float)PD->lua->getArgFloat(2);

    if (rate < 0.01f) rate = 0.01f;

    ZzfxVoice* v = &gVoices[gVoiceCursor];
    gVoiceCursor = (gVoiceCursor + 1) % ZZFX_NUM_VOICES;

    PD->sound->sampleplayer->stop(v->player);
    if (v->sample && v->ownsSample)
        PD->sound->sample->freeSample(v->sample);
    v->sample = gCached[slot].sample;
    v->ownsSample = 0;
    PD->sound->sampleplayer->setSample(v->player, v->sample);
    PD->sound->sampleplayer->play(v->player, 1, rate);
    return 0;
}

static int lua_zzfx_cache_free(lua_State* L)
{
    (void)L;
    int n = PD->lua->getArgCount();
    if (n < 1)
        return 0;

    int id = (int)PD->lua->getArgFloat(1);
    if (id < 1 || id > ZZFX_MAX_CACHED)
        return 0;

    int slot = id - 1;
    if (!gCached[slot].used)
        return 0;

    for (int i = 0; i < ZZFX_NUM_VOICES; i++)
    {
        if (gVoices[i].sample == gCached[slot].sample)
        {
            PD->sound->sampleplayer->stop(gVoices[i].player);
            gVoices[i].sample = NULL;
            gVoices[i].ownsSample = 0;
        }
    }

    if (gCached[slot].sample)
    {
        PD->sound->sample->freeSample(gCached[slot].sample);
        gCached[slot].sample = NULL;
    }
    gCached[slot].used = 0;
    return 0;
}

void zzfx_register_lua(PlaydateAPI* pd)
{
    const char* err = NULL;
    if (!pd->lua->addFunction(lua_zzfx, "__zzfx", &err))
        pd->system->logToConsole("zzfx: addFunction failed: %s", err ? err : "(unknown)");
    if (!pd->lua->addFunction(lua_zzfx_cache_new, "__zzfxCacheNew", &err))
        pd->system->logToConsole("zzfx: addFunction failed: %s", err ? err : "(unknown)");
    if (!pd->lua->addFunction(lua_zzfx_cache_play, "__zzfxCachePlay", &err))
        pd->system->logToConsole("zzfx: addFunction failed: %s", err ? err : "(unknown)");
    if (!pd->lua->addFunction(lua_zzfx_cache_free, "__zzfxCacheFree", &err))
        pd->system->logToConsole("zzfx: addFunction failed: %s", err ? err : "(unknown)");
}
