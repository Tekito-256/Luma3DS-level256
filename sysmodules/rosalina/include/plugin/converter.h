#pragma once

#include <3ds/types.h>
#include "ifile.h"

#define _3GX_MAGIC_OLD (0x3130303024584733) /* "3GX$0001" */

typedef struct CTR_PACKED
{
    u32             authorLen;
    u32             authorMsg;
    u32             titleLen;
    u32             titleMsg;
    u32             summaryLen;
    u32             summaryMsg;
    u32             descriptionLen;
    u32             descriptionMsg;
} _3gx_Infos_Old;

typedef struct CTR_PACKED
{
    u32             count;
    u32             titles;
} _3gx_Targets_Old;

typedef struct CTR_PACKED
{
    u64             magic;
    u32             version;
    u32             codeSize;
    u32             code;
    _3gx_Infos_Old      infos;
    _3gx_Targets_Old    targets;
} _3gx_Header_Old;

int TryToConvertPlugin(IFile *in, IFile *out);