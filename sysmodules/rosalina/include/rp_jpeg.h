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

#include <stdint.h>

enum
{
    RPJPEG_FMT_RGBA8  = 0,
    RPJPEG_FMT_BGR8   = 1,
    RPJPEG_FMT_RGB565 = 2,
    RPJPEG_FMT_RGB5A1 = 3,
    RPJPEG_FMT_RGBA4  = 4,
};

typedef struct RpJpegHuffTable
{
    uint16_t code[256];
    uint8_t size[256];
} RpJpegHuffTable;

typedef struct RpJpeg
{
    int quality;
    RpJpegHuffTable dcY, dcC, acY, acC;
    uint16_t recipY[64];
    uint16_t recipC[64];
    uint8_t dqtY[64];
    uint8_t dqtC[64];
} RpJpeg;

void RpJpeg_SetQuality(RpJpeg *enc, int quality);

uint32_t RpJpeg_BytesPerPixel(int format);

uint32_t RpJpeg_Encode(const RpJpeg *enc, uint8_t *out, uint32_t outCap, const uint8_t *src,
                       uint32_t width, uint32_t rows, uint32_t stride, uint32_t rowStep, int format);
