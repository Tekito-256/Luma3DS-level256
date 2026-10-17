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

#include <3ds/types.h>
#include <3ds/svc.h>
#include "csvc.h"
#include "rp_kcopy.h"

#define ROWS_PER_CALL 40

#define HASH_PRIME 0x01000193u

typedef struct CopyArgs
{
    u32 *dst;
    const u32 *src;
    u32 rows;
    u32 srcPitchWords;
    u32 srcStepRows;
    u32 rowWords;
    u32 hash;
} CopyArgs;

static void copyRows(CopyArgs *args)
{
    u32 *dst = args->dst;
    const u32 *src = args->src;
    u32 h = args->hash;

    for (u32 y = 0; y < args->rows; y++)
    {
        for (u32 i = 0; i < args->rowWords; i++)
        {
            u32 v = src[i];
            dst[i] = v;
            h = (h ^ v) * HASH_PRIME;
        }
        dst += args->rowWords;
        src += args->srcPitchWords * args->srcStepRows;
    }

    args->hash = h;
}

static bool aligned(const void *dst, u32 src, u32 stride, u32 rowBytes)
{
    return (((u32)dst | src | stride | rowBytes) & 3) == 0;
}

bool RpCopy_Rows(void *dst, const void *src, u32 rows, u32 stride, u32 rowStep, u32 rowBytes, u32 *hash)
{
    if (!aligned(dst, (u32)src, stride, rowBytes))
        return false;

    CopyArgs args = {
        .dst = (u32 *)dst,
        .src = (const u32 *)src,
        .rows = rows,
        .srcPitchWords = stride / 4,
        .srcStepRows = rowStep,
        .rowWords = rowBytes / 4,
        .hash = *hash,
    };
    copyRows(&args);
    *hash = args.hash;
    return true;
}

bool RpKernelCopy_Rows(void *dst, u32 kernelVa, u32 rows, u32 stride, u32 rowStep, u32 rowBytes, u32 *hash)
{
    if (!aligned(dst, kernelVa, stride, rowBytes))
        return false;

    u32 h = *hash;
    for (u32 y = 0; y < rows; y += ROWS_PER_CALL)
    {
        CopyArgs args = {
            .dst = (u32 *)dst + y * (rowBytes / 4),
            .src = (const u32 *)(kernelVa + y * rowStep * stride),
            .rows = rows - y < ROWS_PER_CALL ? rows - y : ROWS_PER_CALL,
            .srcPitchWords = stride / 4,
            .srcStepRows = rowStep,
            .rowWords = rowBytes / 4,
            .hash = h,
        };
        svcCustomBackdoor(copyRows, &args);
        h = args.hash;
    }

    *hash = h;
    return true;
}
