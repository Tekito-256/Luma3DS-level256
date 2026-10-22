/*
*   This file is part of Luma3DS (Level256 fork)
*   Copyright (C) 2026 Level256
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <3ds.h>
#include <string.h>
#include "dsp_audio.h"

#define DSP_DATA_VA        0x1FF40000
#define DSP_WORD_VA(w)     (DSP_DATA_VA + 2 * (u32)(w))
#define REGION_WORD(r)     ((r) == 0 ? 0x8000 : 0x18000)
#define FRAME_COUNTER_WORD 0x3FFF
#define FINAL_MIX_WORD     0x0540

#define FC_MAX_STEP        6
#define FC_RESYNC_POLLS    3

static const vu16 *frameCounter(u32 region)
{
    return (const vu16 *)DSP_WORD_VA(REGION_WORD(region) + FRAME_COUNTER_WORD);
}

static const u8 *finalMix(u32 region)
{
    return (const u8 *)DSP_WORD_VA(REGION_WORD(region) + FINAL_MIX_WORD);
}

static u16 readFrameCounter(u32 region)
{
    svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, (u32)frameCounter(region), sizeof(u16));
    return *frameCounter(region);
}

void DspAudio_ReaderInit(DspAudioReader *r)
{
    memset(r, 0, sizeof(*r));
    for (u32 i = 0; i < 2; i++)
        r->lastFc[i] = readFrameCounter(i);
}

static bool stepped(DspAudioReader *r, u32 region, u16 *fc)
{
    u16 now = readFrameCounter(region);
    u16 step = now - r->lastFc[region];
    if (step == 0)
        return false;

    if (step > FC_MAX_STEP || (step & 1) != 0)
    {
        r->oddPolls[region] = now == r->oddFc[region] ? r->oddPolls[region] + 1 : 1;
        r->oddFc[region] = now;
        if (r->oddPolls[region] >= FC_RESYNC_POLLS)
        {
            r->lastFc[region] = now;
            r->oddPolls[region] = 0;
        }
        return false;
    }

    r->lastFc[region] = now;
    r->oddPolls[region] = 0;
    *fc = now;
    return true;
}

u32 DspAudio_Poll(DspAudioReader *r, DspAudioBlock out[2])
{
    u32 n = 0;
    for (u32 region = 0; region < 2; region++)
    {
        u16 fc;
        if (!stepped(r, region, &fc))
            continue;

        svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, (u32)finalMix(region), DSPAUDIO_FRAME_BYTES);
        out[n].frame = fc;
        memcpy(out[n].samples, finalMix(region), DSPAUDIO_FRAME_BYTES);
        n++;
    }

    if (n == 2 && (s16)(out[0].frame - out[1].frame) > 0)
    {
        DspAudioBlock tmp = out[0];
        out[0] = out[1];
        out[1] = tmp;
    }
    return n;
}
