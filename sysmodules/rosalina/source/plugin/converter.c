#include "plugin.h"

#include <string.h>
#include <stdlib.h>

typedef struct {
    u32 pattern;
    u32 mask;
} PatternMask;

static u32 plgLdrDataOffset = 0;
static u32 plgLdrFetchEventOffset = 0;
static u32 plgLdrReplyOffset = 0;
static u32 plgLdrInitOffset = 0;
static u32 initExOffset = 0;
static u32 convertHeaderOffset = 0;

static void ConvertHeader(_3gx_Header *newHeader, const _3gx_Header_Old *oldHeader)
{
    _3gx_Infos *infos = &newHeader->infos;
    _3gx_Executable *executable = &newHeader->executable;
    _3gx_Targets *targets = &newHeader->targets;

    memset(newHeader, 0, sizeof(_3gx_Header));

    newHeader->magic = _3GX_MAGIC;
    newHeader->version = oldHeader->version;
    newHeader->reserved = 0;

    // Infos len
    infos->authorLen = oldHeader->infos.authorLen;
    infos->titleLen = oldHeader->infos.titleLen;
    infos->summaryLen = oldHeader->infos.summaryLen;
    infos->descriptionLen = oldHeader->infos.descriptionLen;

    infos->embeddedExeLoadFunc = 1;
    infos->embeddedSwapSaveLoadFunc = 1;
    //infos->compatibility = 2;
    infos->exeLoadChecksum = 0;

    // Targets
    targets->count = oldHeader->targets.count;

    // Executable
    executable->codeSize = oldHeader->codeSize;
}

static const u32 plgLdrFetchEvent[] =
{
    0xE59F3018,
    0xE5932008,
    0xE1920F9F,
    0xE1823F90,
    0xE6EF3073,
    0xE3530000,
    0x1AFFFFFA,
    0xE12FFF1E,
    0x00000000 // Data offset
};

static const u32 plgLdrReply[] =
{
    0xE92D40F7,
    0xE3A03000,
    0xE1A04000,
    0xE59F50A0,
    0xE595100C,
    0xE1912F9F,
    0xE1812F93,
    0xE6EF2072,
    0xE3520000,
    0x1AFFFFFA,
    0xE3540002,
    0xCA000007,
    0xE5951008,
    0xE1913F9F,
    0xE1813F92,
    0xE6EF3073,
    0xE3530000,
    0x1AFFFFFA,
    0xE28DD00C,
    0xE8BD80F0,
    0xE3A06000,
    0xE3A07000,
    0xE3A03001,
    0xE1CD60F0,
    0xE5950004,
    0x00000000, // svcArbitrateAddress
    0xE3540003,
    0x1A00000D,
    0xE3E02000,
    0xE5951008,
    0xE1913F9F,
    0xE1813F92,
    0xE6EF3073,
    0xE3530000,
    0x1AFFFFFA,
    0xE3A06000,
    0xE3A07000,
    0xE3A02001,
    0xE1CD60F0,
    0xE5950004,
    0x00000000, // svcArbitrateAddress
    0xEAFFFFE7,
    0xE3540004,
    0x1AFFFFE5,
    0xEF000009, // svcExitThread
    0x00000000, // Data offset
};

static const u32 plgLdrInitBin[] =
{
    0xE92D4070,
    0xEE1D5F70,
    0xE59F403C,
    0xE28F103C,
    0xE1A00004,
    0x00000000, // svcConnectToPort
    0xE3A03809,
    0xE5940000,
    0xE5853080,
    0xEF000032,
    0xE3500000,
    0xA595308C,
    0xA5843004,
    0xE3A03407,
    0xE5932018,
    0xE5842008,
    0xE593301C,
    0xE584300C,
    0xE8BD8070,
    0x00000000, // Data offset
    0x3A676C70,
    0x0072646C
};

static const u32 initEx[] =
{
    0xE52DE004, // push {lr}
    0x00000000, // Kernel::Initialize
    0x00000000, // plgLdrInit
    0x00000000, // ConvertHeader
    0xE49DF004, // pop {pc}
};

static const u32 convertHeader[] =
{
    0xE3A03407,
    0xE5932014,
    0xE5832018,
    0xE12FFF1E
};

// From https://gitlab.com/thepixellizeross/ctrpluginframework/-/blame/develop/Library/source/CTRPluginFrameworkImpl/System/HookManager.cpp#L61
static u32 CreateBranchARM(u32 from, u32 to, bool link)
{
    u32 instrBase = (link ? 0xEB000000 : 0xEA000000);
    u32 off = (u32)(to - (from + 8));

    return instrBase | ((off >> 2) & 0xFFFFFF);
}

// From https://github.com/citra-emu/citra/blob/f0a582b218f83f19d98c8df0e6130a32f022368b/src/core/arm/disassembler/arm_disasm.cpp#L314
static u32 GetBranchAddr(u32 addr, u32 value)
{
    if(((value >> 26) & 0x3) != 2)
        return 0;
    
    u8 bit25 = (value >> 25) & 0x1;

    if (bit25 == 0) return 0;

    u32 offset = value & 0xffffff;

    if ((offset >> 23) & 1)
        offset |= 0xff000000;

    offset <<= 2;
    offset += 8;
        
    return addr + offset;
}

static u32 OffsetToVA(const _3gx_Header *header, u32 offset)
{
    u32 codeBaseVa = 0x7000100;
    u32 codeStartOff = header->executable.codeOffset;
    int diff = offset - codeStartOff;

    if (diff < 0)
        return 0;

    return codeBaseVa + diff;
}

void Write(IFile *in, IFile *out, _3gx_Header *header, const _3gx_Header_Old *oldHeader)
{
    static char buffer[4096];
    _3gx_Executable *executable = &header->executable;
    _3gx_Infos *infos = &header->infos;
    _3gx_Targets *targets = &header->targets;
    u64 total;

    IFile_Write(out, &total, header, sizeof(_3gx_Header), 0);

    // Write Infos
    infos->titleMsg = (const char *)(u32)out->pos;
    in->pos = oldHeader->infos.titleMsg;
    IFile_Read(in, &total, buffer, oldHeader->infos.titleLen);
    IFile_Write(out, &total, buffer, oldHeader->infos.titleLen, 0);

    infos->authorMsg = (const char *)(u32)out->pos;
    in->pos = oldHeader->infos.authorMsg;
    IFile_Read(in, &total, buffer, oldHeader->infos.authorLen);
    IFile_Write(out, &total, buffer, oldHeader->infos.authorLen, 0);

    infos->summaryMsg = (const char *)(u32)out->pos;
    in->pos = oldHeader->infos.summaryMsg;
    IFile_Read(in, &total, buffer, oldHeader->infos.summaryLen);
    IFile_Write(out, &total, buffer, oldHeader->infos.summaryLen, 0);

    infos->descriptionMsg = (const char *)(u32)out->pos;
    in->pos = oldHeader->infos.descriptionMsg;
    IFile_Read(in, &total, buffer, oldHeader->infos.descriptionLen);
    IFile_Write(out, &total, buffer, oldHeader->infos.descriptionLen, 0);

    // Write Targets
    targets->titles = (u32 *)(u32)out->pos;
    in->pos = oldHeader->targets.titles;
    IFile_Read(in, &total, buffer, oldHeader->targets.count * sizeof(u32));
    IFile_Write(out, &total, buffer, oldHeader->targets.count * sizeof(u32), 0);

    // Write embedded function
    u32 nop[] = { 0xE3A00000, 0xE320F000 };
    executable->exeLoadFuncOffset = (u32)out->pos;
    executable->swapSaveFuncOffset = (u32)out->pos;
    executable->swapLoadFuncOffset = (u32)out->pos;
    IFile_Write(out, &total, nop, sizeof(nop), 0);

    // Create padding
    executable->codeOffset = (u32)out->pos;
    u32 padding = 16 - (executable->codeOffset & 0xF);
    char zeroes[16] = {0};
    IFile_Write(out, &total, zeroes, padding, 0);
    executable->codeOffset += padding;

    // Copy code
    in->pos = oldHeader->code;

    // Copy code from in to out
    u64 remaining = oldHeader->codeSize;
    do {
        IFile_Read(in, &total, buffer, remaining > 4096 ? 4096 : remaining);
        remaining -= total;
        IFile_Write(out, &total, buffer, total, 0);
    } while(remaining != 0);

    // Write ext data
    static u32 datas[4]; // plgLdrHandle, plgLdrArbiter, plgEvent, plgReply
    plgLdrDataOffset = (u32)out->pos;
    IFile_Write(out, &total, datas, sizeof(datas), 0);
    executable->codeSize += sizeof(datas);

    // Write builtin function
    plgLdrFetchEventOffset = (u32)out->pos;
    IFile_Write(out, &total, plgLdrFetchEvent, sizeof(plgLdrFetchEvent), 0);
    executable->codeSize += sizeof(plgLdrFetchEvent);

    plgLdrReplyOffset = (u32)out->pos;
    IFile_Write(out, &total, plgLdrReply, sizeof(plgLdrReply), 0);
    executable->codeSize += sizeof(plgLdrReply);

    plgLdrInitOffset = (u32)out->pos;
    IFile_Write(out, &total, plgLdrInitBin, sizeof(plgLdrInitBin), 0);
    executable->codeSize += sizeof(plgLdrInitBin);

    initExOffset = (u32)out->pos;
    IFile_Write(out, &total, initEx, sizeof(initEx), 0);
    executable->codeSize += sizeof(initEx);

    convertHeaderOffset = (u32)out->pos;
    IFile_Write(out, &total, convertHeader, sizeof(convertHeader), 0);
    executable->codeSize += sizeof(convertHeader);

    // Rewrite header
    out->pos = 0;
    IFile_Write(out, &total, header, sizeof(_3gx_Header), 0);
}

static int Search(u8 *buffer, u32 bufSize, const PatternMask *pattern, u32 nbPatterns)
{
    if(nbPatterns == 0 || buffer == NULL || bufSize == 0)
        return -1;
    
    for(u32 i = 0; i < bufSize - nbPatterns * sizeof(u32); i++)
    {
        u32 *ptr = (u32 *)(buffer + i);
        bool match = true;

        for(u32 j = 0; j < nbPatterns; j++)
        {
            if((ptr[j] & ~pattern[j].mask) != (pattern[j].pattern & ~pattern[j].mask))
            {
                match = false;
                break;
            }
        }
        if(match)
        {
            return (int)i;
        }
    }

    return -1;
}

static int FileSearch(IFile *file, u32 fileSize, const PatternMask *pattern, u32 nbPatterns)
{
    if(nbPatterns == 0 || file == NULL || fileSize == 0)
        return -1;

    static u8 buffer[4096];
    u32 remaining = fileSize;
    u32 offset = 0;
    u64 total;

    file->pos = 0;
    
    do {
        u32 size = (remaining > 4096 ? 4096 : remaining);
        IFile_Read(file, &total, buffer, size);

        int offs = Search(buffer, total, pattern, nbPatterns);
        if(offs != -1)
            return offset + offs;

        remaining -= total;
        offset += total;

        if(remaining == 0)
            break;

        u32 diff = nbPatterns * sizeof(u32);
        remaining += diff;
        offset -= diff;
        file->pos -= diff;
    } while(remaining != 0);

    return -1;
}

bool Patch(IFile *out, u32 fileSize, const _3gx_Header *header)
{
    static const PatternMask keepThreadMainLoopPattern[] =
    {
        {0xE59D3014, 0},
        {0xE1CD80F0, 0},
        {0xE58D3038, 0},
        {0xE59D3018, 0},
        {0xE3A02002, 0},
        {0xE58D303C, 0},
        {0xE1A01004, 0},
        {0xE3A03000, 0},
        {0xE28D0020, 0},
        {0xE58D5020, 0},
        {0xEB000000, 0xFFFFFF},
        {0xE59D3020, 0},
        {0xE3530000, 0},
        {0x1A00000B, 0},
        {0xEB000000, 0xFFFFFF},
        {0xEAFFFFEF, 0},
    };

    int loopOffset = FileSearch(out, fileSize, keepThreadMainLoopPattern, sizeof(keepThreadMainLoopPattern) / sizeof(PatternMask));

    if(loopOffset == -1)
    {
        return false;
    }

    static const PatternMask svcArbitrateAddressPattern[] =
    {
        {0xE92D0030, 0},
        {0xE59D4008, 0},
        {0xE59D500C, 0},
        {0xEF000022, 0},
        {0xE8BD0030, 0},
        {0xE12FFF1E, 0},
    };

    int svcArbitrateAddressOffset = FileSearch(out, fileSize, (PatternMask *)svcArbitrateAddressPattern, sizeof(svcArbitrateAddressPattern) / sizeof(PatternMask));

    if(svcArbitrateAddressOffset == -1)
    {
        return false;
    }

    static const PatternMask svcConnectToPortPattern[] =
    {
        {0xE52D0004, 0},
        {0xEF00002D, 0},
        {0xE49D3004, 0},
        {0xE5831000, 0},
        {0xE12FFF1E, 0},
    };

    int svcConnectToPortOffset = FileSearch(out, fileSize, svcConnectToPortPattern, sizeof(svcConnectToPortPattern) / sizeof(PatternMask));

    if(svcConnectToPortOffset == -1)
    {
        return false;
    }

    static u8 buffer[4096];
    u64 total;
    u32 tmp;

    // Config plgLdrInit
    out->pos = plgLdrInitOffset;
    IFile_Read(out, &total, buffer, sizeof(plgLdrInitBin));
    u32 *plgLdrInitPtr = (u32 *)buffer;
    plgLdrInitPtr[5] = CreateBranchARM(OffsetToVA(header, plgLdrInitOffset + 5 * sizeof(u32)), OffsetToVA(header, svcConnectToPortOffset), true);
    plgLdrInitPtr[19] = OffsetToVA(header, plgLdrDataOffset);
    out->pos = plgLdrInitOffset;
    IFile_Write(out, &total, buffer, sizeof(plgLdrInitBin), 0);

    // Config plgLdrFetchEvent
    out->pos = plgLdrFetchEventOffset;
    IFile_Read(out, &total, buffer, sizeof(plgLdrFetchEvent));
    u32 *plgLdrFetchEventPtr = (u32 *)buffer;
    plgLdrFetchEventPtr[8] = OffsetToVA(header, plgLdrDataOffset);
    out->pos = plgLdrFetchEventOffset;
    IFile_Write(out, &total, buffer, sizeof(plgLdrFetchEvent), 0);

    // Config plgLdrReply
    out->pos = plgLdrReplyOffset;
    IFile_Read(out, &total, buffer, sizeof(plgLdrReply));
    u32 *plgLdrReplyPtr = (u32 *)buffer;
    plgLdrReplyPtr[25] = CreateBranchARM(OffsetToVA(header, plgLdrReplyOffset + 25 * sizeof(u32)), OffsetToVA(header, svcArbitrateAddressOffset), true);
    plgLdrReplyPtr[40] = CreateBranchARM(OffsetToVA(header, plgLdrReplyOffset + 40 * sizeof(u32)), OffsetToVA(header, svcArbitrateAddressOffset), true);
    plgLdrReplyPtr[45] = OffsetToVA(header, plgLdrDataOffset);
    out->pos = plgLdrReplyOffset;
    IFile_Write(out, &total, buffer, sizeof(plgLdrReply), 0);

    u32 kernelInitOffset = loopOffset - 181 * sizeof(u32);
    out->pos = kernelInitOffset;
    IFile_Read(out, &total, buffer, sizeof(buffer));

    // Init hook
    u32 *kernelOpPtr = (u32 *)buffer;
    u32 kernelInitOp = kernelOpPtr[0];
    u32 kernelInitAddr = GetBranchAddr(OffsetToVA(header, kernelInitOffset), kernelInitOp);
    kernelOpPtr[0] = CreateBranchARM(OffsetToVA(header, kernelInitOffset), OffsetToVA(header, initExOffset), true); // bl initEx
    out->pos = kernelInitOffset;
    IFile_Write(out, &total, buffer, sizeof(buffer), 0);

    // Config initEx
    out->pos = initExOffset;
    IFile_Read(out, &total, buffer, sizeof(initEx));
    u32 *initExPtr = (u32 *)buffer;
    initExPtr[1] = CreateBranchARM(OffsetToVA(header, initExOffset + 1 * sizeof(u32)), kernelInitAddr, true);
    initExPtr[2] = CreateBranchARM(OffsetToVA(header, initExOffset + 2 * sizeof(u32)), OffsetToVA(header, plgLdrInitOffset), true);
    initExPtr[3] = CreateBranchARM(OffsetToVA(header, initExOffset + 3 * sizeof(u32)), OffsetToVA(header, convertHeaderOffset), true);
    out->pos = initExOffset;
    IFile_Write(out, &total, buffer, sizeof(initEx), 0);

    out->pos = loopOffset;
    IFile_Read(out, &total, buffer, sizeof(buffer));
    u32 *ptr = (u32 *)buffer;
    *ptr++ = 0xE59D0014; // ldr r0, =memLayoutChanged
    *ptr++ = 0xE59F20AC; // ldr r2, =100000000
    *ptr++ = 0xE3A03000; // mov r3, #0
    *ptr++ = 0xEF000024; // svcWaitSynchronization
    *ptr++ = 0xE59F50A4; // ldr r5, =0x9401BFE
    *ptr++ = 0xE1500005; // cmp r5, r0
    *ptr++ = 0x1A000006; // bne MemoryLayoutChangedHandler
    tmp = (u32)ptr;
    *ptr++ = CreateBranchARM(OffsetToVA(header, loopOffset + (tmp - (u32)buffer)), OffsetToVA(header, plgLdrFetchEventOffset), true); // bl PLGLDR__Fetch
    *ptr++ = 0xE3500004; // cmp r0, #4
    *ptr++ = 0x0A00000F; // beq Exit
    tmp = (u32)ptr;
    *ptr++ = CreateBranchARM(OffsetToVA(header, loopOffset + (tmp - (u32)buffer)), OffsetToVA(header, plgLdrReplyOffset), true); // bl PLGLDR__Reply
    *ptr++ = 0xEAFFFFF3; // b start
    *ptr++ = 0xE320F000;
    *ptr++ = 0xE320F000;
    ptr++; // bl UpdateMemRegions
    ptr++; // b loopBegin
    out->pos = loopOffset;
    IFile_Write(out, &total, buffer, sizeof(buffer), 0);

    u32 exitProcessOffset = loopOffset + 44 * sizeof(u32);
    out->pos = exitProcessOffset;
    IFile_Read(out, &total, buffer, sizeof(buffer));
    u32 *exitProcessPtr = (u32 *)buffer;
    *exitProcessPtr++ = 0xE3A00004; // mov r0, #4
    tmp = (u32)exitProcessPtr;
    *exitProcessPtr++ = CreateBranchARM(OffsetToVA(header, exitProcessOffset + (tmp - (u32)buffer)), OffsetToVA(header, plgLdrReplyOffset), true); // bl PLGLDR__Reply
    *exitProcessPtr++ = 0x05F5E100;
    *exitProcessPtr++ = 0x09401BFE;
    out->pos = exitProcessOffset;
    IFile_Write(out, &total, buffer, sizeof(buffer), 0);

    return true;
}

int TryToConvertPlugin(IFile *in, IFile *out)
{
    _3gx_Header_Old oldHeader;
    _3gx_Header newHeader;
    u64 fileSize;
    u64 total;

    memset(&newHeader, 0, sizeof(_3gx_Header));
    memset(&oldHeader, 0, sizeof(_3gx_Header_Old));

    in->pos = 0;
    IFile_Read(in, &total, (char *)&oldHeader, sizeof(_3gx_Header_Old));

    if(oldHeader.magic != _3GX_MAGIC_OLD)
    {
        return 1;
    }

    ConvertHeader(&newHeader, &oldHeader);
    Write(in, out, &newHeader, &oldHeader);

    // Get size
    IFile_GetSize(out, &fileSize);

    if(!Patch(out, (u32)fileSize, &newHeader))
    {
        return 2;
    }

    return 0;
}
