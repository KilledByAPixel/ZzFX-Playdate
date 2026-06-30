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

// Leading silence (in samples) prepended to every buffer, to absorb a click the
// sample player injects shortly after it starts a sound (the audio output
// settling as it wakes from idle -- the pop lands ~3-11 ms in, so the silence
// must cover that span). 512 ~= 11.6 ms at 44100; 128 (~3 ms) was too short.
// 11.6 ms of start latency is below the ~20-30 ms threshold of perception.
#define ZZFX_LEAD_SILENCE  512

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
static float  sgnf(float v)              { return v < 0.0f ? -1.0f : 1.0f; }

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
    // The synth runs in SINGLE precision (float) so it uses the Playdate's
    // hardware FPU. The Cortex-M7 has no double-precision FPU, so doing this in
    // double falls back to slow software emulation -- on device that overruns
    // the watchdog while building the cached sounds (seconds of work -> black
    // screen / reset). float is ~10-50x faster here and inaudibly different for
    // these short sound effects. The one-time parameter scaling below is done
    // in double for accuracy, then stored as float for the per-sample loop.
    const float PI2 = (float)(M_PI * 2.0);
    const double dSR = (double)ZZFX_SAMPLE_RATE;
    const double dPI2 = M_PI * 2.0;

    // init parameters (mirrors the JS, line for line)
    float fslide = (float)(slide * (500.0 * dPI2 / dSR / dSR));
    float startSlide = fslide;
    float ffreq = (float)(frequency *
        (1.0 + randomness * 2.0 * zzfx_rand() - randomness) * dPI2 / dSR);
    float startFrequency = ffreq;
    float modOffset = 0.0f;
    long  repeat = 0, crush = 0, jump = 1;
    float t = 0.0f, s = 0.0f, f;

    // biquad LP/HP filter coefficients (computed once; stored as float)
    float ffilter = (float)filter;
    float quality = 2.0f, w = PI2 * fabsf(ffilter) * 2.0f / (float)dSR;
    float cosw = cosf(w), alpha = sinf(w) / 2.0f / quality;
    float a0 = 1.0f + alpha, a1 = -2.0f * cosw / a0, a2 = (1.0f - alpha) / a0;
    float b0 = (1.0f + sgnf(ffilter) * cosw) / 2.0f / a0;
    float b1 = -(sgnf(ffilter) + cosw) / a0, b2 = b0;
    float x2 = 0, x1 = 0, y2 = 0, y1 = 0;

    // scale by sample rate (float for the per-sample loop)
    const float minAttack = 9.0f;          // prevent pop if attack is 0
    float fattack = (float)(attack * dSR);
    if (fattack == 0.0f) fattack = minAttack;
    float fdecay   = (float)(decay   * dSR);
    float fsustain = (float)(sustain * dSR);
    float frelease = (float)(release * dSR);
    float fdelay   = (float)(delay   * dSR);
    float fdeltaSlide   = (float)(deltaSlide * (500.0 * dPI2 / (dSR * dSR * dSR)));
    float fmodulation   = (float)(modulation * (dPI2 / dSR));
    float fpitchJump    = (float)(pitchJump  * (dPI2 / dSR));
    float fpitchJumpTime = (float)(pitchJumpTime * dSR);
    long  repeatTimeI = (long)(repeatTime * dSR);          // | 0
    float fvolume = (float)(volume * ZZFX_BUILD_VOLUME);
    float fshapeCurve    = (float)shapeCurve;
    float fsustainVolume = (float)sustainVolume;
    float ftremolo = (float)tremolo;
    float fnoise   = (float)noise;

    // precomputed reciprocals: turn the per-sample envelope divides into cheaper
    // multiplies (float divide is ~14 cycles on the M7 FPU, multiply ~1-3).
    const float invAttack   = 1.0f / fattack;                                    // fattack >= 9, never 0
    const float decayCoef   = (fdecay   > 0.0f) ? (1.0f - fsustainVolume) / fdecay   : 0.0f;
    const float releaseCoef = (frelease > 0.0f) ? fsustainVolume / frelease         : 0.0f;

    int sh = (int)shape;
    int crushStep = (int)(bitCrush * 100.0);              // bitCrush*100 | 0

    int length = (int)(fattack + fdecay + fsustain + frelease + fdelay);  // | 0
    if (length < 1) length = 1;

    // float work buffer (matches JS b[]); used for delay feedback too
    float* b = (float*)PD->system->realloc(NULL, sizeof(float) * (size_t)length);
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
            if      (sh == 0) s = sinf(t);                                            // sin
            else if (sh == 1) s = 1.0f - 4.0f * fabsf(floorf(t / PI2 + 0.5f) - t / PI2);// triangle
            else if (sh == 2) s = 1.0f - fmodf(fmodf(2.0f * t / PI2, 2.0f) + 2.0f, 2.0f);// saw
            else if (sh == 3) { float tv = tanf(t); s = tv > 1.0f ? 1.0f : (tv < -1.0f ? -1.0f : tv); } // tan
            else if (sh == 4) s = sinf(t * t * t);                                    // noise
            else              s = (fmodf(t / PI2, 1.0f) < fshapeCurve / 2.0f) ? 1.0f : -1.0f; // square duty

            float trem = repeatTimeI
                ? (1.0f - ftremolo + ftremolo * sinf(PI2 * (float)i / (float)repeatTimeI))
                : 1.0f;

            // shapeCurve == 1 is the common case (x^1 == x), so skip the costly powf
            float shaped = (sh > 4 || fshapeCurve == 1.0f) ? s : sgnf(s) * powf(fabsf(s), fshapeCurve);

            float env;
            if      ((float)i <= fattack) {
                // Include the attack boundary sample so the ramp reaches 1.0
                // before leaving attack, avoiding a small discontinuity.
                env = (float)i * invAttack;
            }
            else if ((float)i < fattack + fdecay && fdecay > 0.0f) {
                env = 1.0f - ((float)i - fattack) * decayCoef;
            }
            else if ((float)i < fattack + fdecay + fsustain) {
                env = fsustainVolume;
            }
            else if ((float)i < (float)length - fdelay) {
                env = ((float)length - (float)i - fdelay) * releaseCoef;
            }
            else {
                env = 0.0f;
            }

            if (env < 0.0f) env = 0.0f;
            if (env > 1.0f) env = 1.0f;

            s = trem * shaped * env;

            // delay
            if (fdelay != 0.0f)
            {
                float dterm;
                if (fdelay > (float)i) dterm = 0.0f;
                else {
                    float rel = ((float)i < (float)length - fdelay) ? 1.0f : ((float)length - (float)i) / fdelay;
                    dterm = rel * b[(long)((float)i - fdelay)] / 2.0f / fvolume;
                }
                s = s / 2.0f + dterm;
            }

            // biquad filter (assignments evaluated left-to-right as in JS)
            if (ffilter != 0.0f)
            {
                float X2 = x2, X1 = x1, Y2 = y2, Y1 = y1;
                float out = b2 * X2 + b1 * X1 + b0 * s - a2 * Y2 - a1 * Y1;
                x2 = X1;   // x2 = x1
                x1 = s;    // x1 = s
                y2 = Y1;   // y2 = y1
                y1 = out;  // y1 = result
                s = out;
            }
        }

        b[i] = s * fvolume;   // store sample (JS: b[i++] = s * volume)

        // advance oscillator. The modulation cos and noise sin/pow are skipped
        // when those params are 0 (the common case) -- mathematically identical
        // (cos(0)=1, the noise term is *0) but avoids two transcendentals/sample.
        fslide += fdeltaSlide;
        ffreq  += fslide;
        f = (fmodulation != 0.0f) ? ffreq * cosf(fmodulation * modOffset) : ffreq;
        modOffset += 1.0f;
        t += f;
        if (fnoise != 0.0f)
            t += f * fnoise * sinf(powf((float)i, 5.0f));

        // pitch jump
        if (jump && (++jump > fpitchJumpTime))
        {
            ffreq          += fpitchJump;
            startFrequency += fpitchJump;
            jump = 0;
        }

        // repeat
        if (repeatTimeI && ((++repeat % repeatTimeI) == 0))
        {
            ffreq  = startFrequency;
            fslide = startSlide;
            if (!jump) jump = 1;
        }
    }

    // convert to 16-bit PCM, applying the playback master gain and clamping.
    // A block of leading silence (ZZFX_LEAD_SILENCE) is prepended so any click
    // the sample player injects at start-of-playback lands in the silence.
    const int pad = ZZFX_LEAD_SILENCE;
    int16_t* pcm = (int16_t*)PD->system->realloc(NULL, sizeof(int16_t) * (size_t)(length + pad));
    if (!pcm) { PD->system->realloc(b, 0); *outLen = 0; return NULL; }

    for (int i = 0; i < pad; i++) pcm[i] = 0;     // leading silence

    for (int i = 0; i < length; i++)
    {
        float v = b[i] * (float)ZZFX_PLAYBACK_VOLUME;
        if (v != v) v = 0.0f;                 // NaN guard
        if (v >  1.0f) v =  1.0f;
        if (v < -1.0f) v = -1.0f;
        pcm[pad + i] = (int16_t)(v * 32767.0f);
    }

    PD->system->realloc(b, 0);
    *outLen = length + pad;
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
