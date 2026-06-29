//
//  main.c - entry point for the ZzFX Playdate demo.
//
//  We do NOT install a C update callback, so after eventHandler returns the
//  SDK loads and runs Source/main.lua and drives playdate.update() from Lua.
//  We use kEventInitLua to start the synth and register the global __zzfx()
//  function that Source/zzfx.lua wraps as zzfx({...}).
//
#include "pd_api.h"
#include "zzfx.h"

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg)
{
    (void)arg;
    switch (event)
    {
        case kEventInitLua:
            zzfx_init(pd);
            zzfx_register_lua(pd);
            break;
        default:
            break;
    }
    return 0;
}
