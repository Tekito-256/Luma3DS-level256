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


#pragma once

#include <3ds/types.h>

#define DSPAUDIO_SAMPLE_RATE   32728
#define DSPAUDIO_FRAME_SAMPLES 160
#define DSPAUDIO_FRAME_BYTES   (DSPAUDIO_FRAME_SAMPLES * 2 * sizeof(s16))

typedef struct DspAudioReader
{
    u16 lastFc[2];
    u16 oddFc[2];
    u8 oddPolls[2];
} DspAudioReader;

typedef struct DspAudioBlock
{
    u16 frame;
    u8 samples[DSPAUDIO_FRAME_BYTES] __attribute__((aligned(4)));
} DspAudioBlock;

void DspAudio_ReaderInit(DspAudioReader *r);

u32 DspAudio_Poll(DspAudioReader *r, DspAudioBlock out[2]);
