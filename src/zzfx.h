//
//  zzfx.h - ZzFX sound effect synth, ported to the Playdate C API
//
//  ZzFX - Zuper Zmall Zound Zynth by Frank Force
//  https://github.com/KilledByAPixel/ZzFX  (MIT License)
//
//  This is a faithful C port of ZZFX.buildSamples(): given the same 21
//  parameters as the JavaScript library, it synthesizes a 16-bit PCM buffer
//  at 44100 Hz (the Playdate's native audio rate) and plays it through a
//  pool of SamplePlayers. No sound asset files required.
//
#ifndef ZZFX_H
#define ZZFX_H

#include "pd_api.h"

// Call once, from the kEventInitLua (or kEventInit) handler.
void zzfx_init(PlaydateAPI* pd);

// Register the global Lua function so Lua code can call zzfx({...}).
// Call from the kEventInitLua handler, after zzfx_init().
void zzfx_register_lua(PlaydateAPI* pd);

// Play a sound directly from C. Pass all 21 ZzFX parameters in order:
//   0 volume        7 shapeCurve    14 modulation
//   1 randomness    8 slide         15 bitCrush
//   2 frequency     9 deltaSlide    16 delay
//   3 attack       10 pitchJump     17 sustainVolume
//   4 sustain      11 pitchJumpTime 18 decay
//   5 release      12 repeatTime    19 tremolo
//   6 shape        13 noise         20 filter
void zzfx_play(const double params[21]);

#endif // ZZFX_H
