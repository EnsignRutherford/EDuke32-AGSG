#pragma once
// GameStation Go diagnostics: engine-call time accumulators, active only while the game tick runs.
#ifdef EDUKE32_GAMESTATION_GO
#include "timer.h"

enum { GSGE_CLIPMOVE, GSGE_GETZRANGE, GSGE_HITSCAN, GSGE_CANSEE, GSGE_UPDSECTOR, GSGE_NEARTAG, GSGE_COUNT };

extern volatile int gsgProfInTic;
extern uint64_t g_gsgEngNs[GSGE_COUNT];
extern uint32_t g_gsgEngCnt[GSGE_COUNT];
extern uint32_t g_gsgInsideCalls, g_gsgInsideWalls;

struct GsgEngScope
{
    int      id;
    uint64_t t0;
    explicit GsgEngScope(int i) : id(gsgProfInTic ? i : -1), t0(0)
    {
        if (id >= 0)
            t0 = timerGetNanoTicks();
    }
    ~GsgEngScope()
    {
        if (id >= 0)
        {
            g_gsgEngNs[id] += timerGetNanoTicks() - t0;
            g_gsgEngCnt[id]++;
        }
    }
};
#define GSG_ENG_SCOPE(i) GsgEngScope gsgEngScope__(i)
#else
#define GSG_ENG_SCOPE(i) ((void)0)
#endif
