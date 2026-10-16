#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include "csvc.h"
#include "plugin.h"
#include "ifile.h"
#include "ifile.h"
#include "utils.h"
#include "menu.h"

// Use a global to avoid stack overflow, those structs are quite heavy
static FS_DirectoryEntry   g_entries[10];

static char        g_path[256];
static const char *g_dirPath = "/luma/plugins/%016llX";
static const char *g_defaultPath = "/luma/plugins/default.3gx";
static const char *g_level256PluginPath = "/luma/plugins/Level256_Network/Level256.3gx";

// pluginLoader.s
void        gamePatchFunc(void);

static u32     strlen16(const u16 *str)
{
    if (!str) return 0;

    const u16 *strEnd = str;

    while (*strEnd) ++strEnd;

    return strEnd - str;
}

static Result   FindPluginFile(u64 tid)
{
    char                filename[256];
    u32                 entriesNb = 0;
    bool                found = false;
    Handle              dir = 0;
    Result              res;
    FS_Archive          sdmcArchive = 0;
    FS_DirectoryEntry * entries = g_entries;
    
    memset(entries, 0, sizeof(g_entries));
    sprintf(g_path, g_dirPath, tid);

    if (R_FAILED((res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))))
        goto exit;

    if (R_FAILED((res = FSUSER_OpenDirectory(&dir, sdmcArchive, fsMakePath(PATH_ASCII, g_path)))))
        goto exit;

    strcat(g_path, "/");
    while (!found && R_SUCCEEDED(FSDIR_Read(dir, &entriesNb, 10, entries)))
    {
        if (entriesNb == 0)
            break;

        static const u16 *   validExtension = u"3gx";

        for (u32 i = 0; i < entriesNb; ++i)
        {
            FS_DirectoryEntry *entry = &entries[i];

            // If entry is a folder, skip it
            if (entry->attributes & 1)
                continue;

            // Check extension
            u32 size = strlen16(entry->name);
            if (size <= 5)
                continue;

            u16 *fileExt = entry->name + size - 3;

            if (memcmp(fileExt, validExtension, 3 * sizeof(u16)))
                continue;

            // Convert name from utf16 to utf8
            int units = utf16_to_utf8((u8 *)filename, entry->name, 100);
            if (units == -1)
                continue;
            filename[units] = 0;
            found = true;
            break;
        }
    }

    if (!found)
        res = MAKERESULT(28, 4, 0, 1018);
    else
    {
        u32 len = strlen(g_path);
        filename[256 - len] = 0;
        strcat(g_path, filename);
        PluginLoaderCtx.pluginPath = g_path;
    }

exit:
    FSDIR_Close(dir);
    FSUSER_CloseArchive(sdmcArchive);

    return res;
}

static Result   OpenFile(IFile *file, const char *path)
{
    return IFile_Open(file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ);
}

static bool     IsLevel256SupportedTitle(u64 tid)
{
    switch (tid)
    {
    case 0x000400000016C700ULL: // 赤猫団
    case 0x000400000016C600ULL: // 白犬隊
    case 0x00040000001CF000ULL: // Red cats (EUR)
    case 0x00040000001CEC00ULL: // White dogs (EUR)
    case 0x0004000000191000ULL: // スシ
    case 0x0004000000191100ULL: // テンプラ
    case 0x00040000001AF400ULL: // スキヤキ
    case 0x000400000012F900ULL: // 元祖
    case 0x000400000012F800ULL: // 本家
    case 0x0004000000155100ULL: // 真打
    case 0x00040000001B2900ULL: // 真打(EUR)
    case 0x000400000019AB00ULL: // 元祖(EUR)
    case 0x000400000019AC00ULL: // 本家(EUR)
    case 0x000400000019AA00ULL: // 妖怪ウォッチ2 (USA)
    case 0x000400000019A900ULL: // 妖怪ウォッチ2 (USA)
    case 0x00040000001B2700ULL: // 妖怪ウォッチ2 真打 (USA)
    case 0x00040000001D6800ULL: // 妖怪ウォッチ3 (EUR)
    case 0x00040000001D6900ULL: // 妖怪ウォッチ3 France
    case 0x00040000001D6A00ULL: // 妖怪ウォッチ3 German
    case 0x00040000001D6B00ULL: // 妖怪ウォッチ3 Italy
    case 0x00040000001D6C00ULL: // 妖怪ウォッチ3 Spain
    case 0x00040000001D6700ULL: // 妖怪ウォッチ3 (USA)
    case 0x00040000001CEB00ULL: // 妖怪ウォッチバスターズ (USA)
    case 0x00040000001CEF00ULL: // 妖怪ウォッチバスターズ (USA)
    case 0x000400000019B000ULL: // 妖怪ウォッチ2 (オーストラリア)
    case 0x000400000019B100ULL: // 妖怪ウォッチ2 (オーストラリア)
    case 0x00040000001B2800ULL: // 妖怪ウォッチ2真打 (オーストラリア)
    case 0x000400000019AE00ULL: // 妖怪ウォッチ2 (Kor)
    case 0x000400000019AF00ULL: // 妖怪ウォッチ2 (Kor)
    case 0x00040000001B2A00ULL: // 妖怪ウォッチ2真打 (Kor)
    case 0x00040000001CF200ULL: // 妖怪ウォッチバスターズ (Kor)
    case 0x00040000001CED00ULL: // 妖怪ウォッチバスターズ (Kor)
    case 0x00040000000CF400ULL: // 妖怪ウォッチ1
    case 0x0004000000167800ULL: // 妖怪ウォッチ1 (EUR)
    case 0x0004000000167700ULL: // 妖怪ウォッチ1 (USA)
    case 0x000400000017C200ULL: // 妖怪ウォッチ1 (Au)
    case 0x0004000000167600ULL: // 妖怪ウォッチ1 (Kor)
    case 0x0004000000086200ULL: // とび森
    case 0x0004000000198D00ULL: // とび森 Amiibo+
    case 0x0004000000086400ULL: // とび森 (EUR)
    case 0x0004000000198F00ULL: // とび森 (EUR) Amiibo+
    case 0x0004000000086300ULL: // とび森 (USA)
    case 0x0004000000198E00ULL: // とび森 (USA) Amiibo+
    case 0x0004000000086500ULL: // とび森 (KOR)
    case 0x0004000000199000ULL: // とび森 (KOR) Amiibo+
    case 0x00040000001C9400ULL: // 妖怪ウォッチバスターズ2
    case 0x00040000001C9C00ULL: // 妖怪ウォッチバスターズ2
    case 0x000400000018B000ULL: // 妖怪三国志
    case 0x0004000000197100ULL: // MHXX (JP)
        return true;
    default:
        return false;
    }
}

static Result   OpenPluginFile(u64 tid, IFile *plugin)
{
    // Level256 plugin takes priority on supported titles (hold SELECT to skip)
    if (!(HID_PAD & KEY_SELECT) && IsLevel256SupportedTitle(tid) && R_SUCCEEDED(OpenFile(plugin, g_level256PluginPath)))
    {
        PluginLoaderCtx.pluginPath = g_level256PluginPath;
        return 0;
    }

    if (R_FAILED(FindPluginFile(tid)) || OpenFile(plugin, g_path))
    {
        // Try to open default plugin
        if (OpenFile(plugin, g_defaultPath))
            return -1;

        PluginLoaderCtx.pluginPath = g_defaultPath;
        PluginLoaderCtx.header.isDefaultPlugin = 1;
        return 0;
    }

    return 0;
}

static Result   CheckPluginCompatibility(_3gx_Header *header, u32 processTitle)
{
    static char   errorBuf[0x100];

    if (header->targets.count == 0)
        return 0;

    for (u32 i = 0; i < header->targets.count; ++i)
    {
        if (header->targets.titles[i] == processTitle)
            return 0;
    }

    sprintf(errorBuf, "The plugin - %s -\nis not compatible with this game.\n" \
                      "Contact \"%s\" for more infos.", header->infos.titleMsg, header->infos.authorMsg);
    
    PluginLoaderCtx.error.message = errorBuf;

    return -1;
}

bool     TryToLoadPlugin(Handle process, bool isHomebrew)
{
    u64             tid;
    u64             fileSize;
    IFile           plugin;
    Result          res;
    _3gx_Header     fileHeader;
    _3gx_Header     *header = NULL;
    _3gx_Executable *exeHdr = NULL;
    PluginLoaderContext *ctx = &PluginLoaderCtx;
    PluginHeader        *pluginHeader = &ctx->header;
    const u32           memRegionSizes[] = 
    {
        5 * 1024 * 1024, // 5 MiB
        2 * 1024 * 1024, // 2 MiB
        10 * 1024 * 1024,  // 10 MiB
        5 * 1024 * 1024, // 5 MiB (Reserved)
    };

    // Get title id
    svcGetProcessInfo((s64 *)&tid, process, 0x10001);

    memset(pluginHeader, 0, sizeof(PluginHeader));
    pluginHeader->magic = HeaderMagic;

    // Try to open plugin file
    if (ctx->useUserLoadParameters && (ctx->userLoadParameters.lowTitleId == 0 || (u32)tid == ctx->userLoadParameters.lowTitleId))
    {
        if (!ctx->userLoadParameters.persistent) ctx->useUserLoadParameters = false;
        ctx->pluginMemoryStrategy = ctx->userLoadParameters.pluginMemoryStrategy;
        if (OpenFile(&plugin, ctx->userLoadParameters.path))
            return false;

        ctx->pluginPath = ctx->userLoadParameters.path;

        memcpy(pluginHeader->config, ctx->userLoadParameters.config, 32 * sizeof(u32));
    }
    else
    {
        if (R_FAILED(OpenPluginFile(tid, &plugin)))
            return false;
    }

    if (R_FAILED((res = IFile_GetSize(&plugin, &fileSize))))
        ctx->error.message = "Couldn't get file size";

    if (!res && R_FAILED(res = Check_3gx_Magic(&plugin)))
    {
        const char * errors[] = 
        {
            "Couldn't read file.",
            "Invalid plugin file\nNot a valid 3GX plugin format!",
            "Outdated plugin file\nCheck for an updated plugin.",
            "Outdated plugin loader\nCheck for Luma3DS updates."   
        };

        ctx->error.message = errors[R_MODULE(res) == RM_LDR ? R_DESCRIPTION(res) : 0];
    }

    // Read header
    if (!res && R_FAILED((res = Read_3gx_Header(&plugin, &fileHeader))))
        ctx->error.message = "Couldn't read file";

    // Check compatibility
    if (!res && fileHeader.infos.compatibility == PLG_COMPAT_EMULATOR) {
        ctx->error.message = "Plugin is only compatible with emulators";
        res = -1;
    }

    // Check if plugin can load on homebrew
    if (!res && (isHomebrew && !fileHeader.infos.allowHomebrewLoad)) {
        // Do not display message as this is a common case
        ctx->error.message = NULL;
        res = -1;
    }

    // Flags
    if (!res) {
        ctx->eventsSelfManaged = fileHeader.infos.eventsSelfManaged;
        ctx->isMemPrivate = fileHeader.infos.usePrivateMemory;
        if (ctx->pluginMemoryStrategy == PLG_STRATEGY_SWAP && fileHeader.infos.swapNotNeeded)
            ctx->pluginMemoryStrategy = PLG_STRATEGY_NONE;
    }

    // Set memory region size according to header
    if (!res && R_FAILED((res = MemoryBlock__SetSize(memRegionSizes[fileHeader.infos.memoryRegionSize])))) {
        ctx->error.message = "Couldn't set memblock size.";
    }
    
    // Ensure memory block is mounted
    if (!res && R_FAILED((res = MemoryBlock__IsReady())))
        ctx->error.message = "Failed to allocate memory.";

    // Plugins will not exceed 5MB so this is fine
    if (!res) {
        header = (_3gx_Header *)(ctx->memblock.memblock + g_memBlockSize - (u32)fileSize);
        memcpy(header, &fileHeader, sizeof(_3gx_Header));
    }

    // Parse rest of header
    if (!res && R_FAILED((res = Read_3gx_ParseHeader(&plugin, header))))
        ctx->error.message = "Couldn't read file";

    // Read embedded save/load functions
    if (!res && R_FAILED((res = Read_3gx_EmbeddedPayloads(&plugin, header))))
        ctx->error.message = "Invalid save/load payloads.";
    
    // Save exe checksum
    if (!res)
        ctx->exeLoadChecksum = header->infos.exeLoadChecksum;
    
    // Check titles compatibility
    if (!res) res = CheckPluginCompatibility(header, (u32)tid);

    // Read code
    if (!res && R_FAILED(res = Read_3gx_LoadSegments(&plugin, header, ctx->memblock.memblock + sizeof(PluginHeader)))) {
        if (res == MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_NO_DATA)) ctx->error.message = "This plugin requires a loading function.";
        else if (res == MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_INVALID_ADDRESS)) ctx->error.message = "This plugin file is corrupted.";
        else ctx->error.message = "Couldn't read plugin's code";
    }

    if (R_FAILED(res))
    {
        ctx->error.code = res;
        goto exitFail;
    }

    pluginHeader->version = header->version;
    pluginHeader->waitForReplyTimeout = 10000000000ULL;
    // Code size must be page aligned
    exeHdr = &header->executable;
    pluginHeader->exeSize = (sizeof(PluginHeader) + exeHdr->codeSize + exeHdr->rodataSize + exeHdr->dataSize + exeHdr->bssSize + 0x1000) & ~0xFFF;
    pluginHeader->heapVA = 0x06000000;
    pluginHeader->heapSize = g_memBlockSize - pluginHeader->exeSize;
    pluginHeader->plgldrEvent = ctx->plgEventPA;
    pluginHeader->plgldrReply = ctx->plgReplyPA;

    // Clear old event data
    PLG__NotifyEvent(PLG_OK, false);

    // Copy header to memblock
    memcpy(ctx->memblock.memblock, pluginHeader, sizeof(PluginHeader));
    // Clear heap
    memset(ctx->memblock.memblock + pluginHeader->exeSize, 0, pluginHeader->heapSize);

    // Enforce RWX mmu mapping
    svcControlProcess(process, PROCESSOP_SET_MMU_TO_RWX, 0, 0);
    // Ask the kernel to signal when the process is about to be terminated
    svcControlProcess(process, PROCESSOP_SIGNAL_ON_EXIT, 0, 0);

    // Mount the plugin memory in the process
    if (R_SUCCEEDED(MemoryBlock__MountInProcess()))
    // Install hook
    {
        u32  procStart = 0x00100000;
        u32 *game = (u32 *)procStart;

        extern u32  g_savedGameInstr[2];

        if (R_FAILED((res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, procStart, process, procStart, 0x1000, 0))))
        {
            ctx->error.message = "Couldn't map process";
            ctx->error.code = res;
            goto exitFail;
        }

        g_savedGameInstr[0] = game[0];
        g_savedGameInstr[1] = game[1];

        game[0] = 0xE51FF004; // ldr pc, [pc, #-4]
        game[1] = (u32)PA_FROM_VA_PTR(gamePatchFunc);
        svcFlushEntireDataCache();
        svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, procStart, 0x1000);
    }
    else
        goto exitFail;


    IFile_Close(&plugin);
    return true;

exitFail:
    IFile_Close(&plugin);
    MemoryBlock__Free();

    return false;
}
