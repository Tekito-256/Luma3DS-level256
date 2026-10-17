#include <3ds.h>
#include <string.h>
#include "plugin.h"
#include "plugin/l256_lfcs.h"
#include "ifile.h"
#include "utils.h"

#define PS_INIT_ATTEMPTS    5
#define PS_INIT_RETRY_NS    (20 * 1000 * 1000LL)

static const char *g_lfcsPaths[] = { "/sys/LocalFriendCodeSeed_B", "/sys/LocalFriendCodeSeed_A" };
static const u32 g_lfcsSources[] = { L256_LFCS_SOURCE_B, L256_LFCS_SOURCE_A };

static Result L256_ReadSeedFile(const char *path, u8 *out)
{
    IFile file;
    u64 size;
    u64 total;
    Result res = IFile_Open(&file, ARCHIVE_NAND_RW, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ);

    if (R_FAILED(res))
        return res;

    if (R_SUCCEEDED(res = IFile_GetSize(&file, &size)) && size < L256_LFCS_BLOB_SIZE)
        res = L256_RESULT_LFCS_NOT_FOUND;

    if (R_SUCCEEDED(res) && R_SUCCEEDED(res = IFile_Read(&file, &total, out, L256_LFCS_BLOB_SIZE)) && total != L256_LFCS_BLOB_SIZE)
        res = L256_RESULT_LFCS_NOT_FOUND;

    IFile_Close(&file);
    return res;
}

Result L256_ReadLfcsBlob(u8 *out, u32 *source)
{
    static u8 blob[L256_LFCS_BLOB_SIZE];

    for (u32 i = 0; i < sizeof(g_lfcsPaths) / sizeof(g_lfcsPaths[0]); i++)
    {
        if (R_SUCCEEDED(L256_ReadSeedFile(g_lfcsPaths[i], blob)))
        {
            memcpy(out, blob, L256_LFCS_BLOB_SIZE);
            memset(blob, 0, sizeof(blob));
            *source = g_lfcsSources[i];
            return 0;
        }
    }

    memset(blob, 0, sizeof(blob));
    memset(out, 0, L256_LFCS_BLOB_SIZE);
    *source = L256_LFCS_SOURCE_NONE;
    return L256_RESULT_LFCS_NOT_FOUND;
}

Result L256_GetRandomBytes(u8 *out, u32 size)
{
    static u8 buffer[L256_RANDOM_BYTES_MAX];
    Result res = 0;

    if (size > sizeof(buffer))
        return MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_LDR, RD_TOO_LARGE);

    for (u32 attempt = 0; attempt < PS_INIT_ATTEMPTS; attempt++)
    {
        if (attempt != 0)
            svcSleepThread(PS_INIT_RETRY_NS);
        if (R_SUCCEEDED(res = psInit()))
            break;
    }

    if (R_SUCCEEDED(res))
    {
        res = PS_GenerateRandomBytes(buffer, size);
        psExit();
    }

    if (R_SUCCEEDED(res))
        memcpy(out, buffer, size);
    else
        memset(out, 0, size);

    memset(buffer, 0, sizeof(buffer));
    return res;
}

static Result L256_AuthorizeLfcsRequest(u32 callerPid)
{
    PluginLoaderContext *ctx = &PluginLoaderCtx;
    u32 targetPid;

    if (ctx->target == 0 || !ctx->isLevel256Plugin)
        return L256_RESULT_NOT_AUTHORIZED;

    if (R_FAILED(svcGetProcessId(&targetPid, ctx->target)) || targetPid != callerPid)
        return L256_RESULT_NOT_AUTHORIZED;

    if (ctx->lfcsRequestCount >= L256_LFCS_REQUESTS_PER_LOAD)
        return L256_RESULT_RATE_LIMITED;

    ctx->lfcsRequestCount++;
    return 0;
}

void L256_HandleGetDeviceLfcsBlob(u32 *cmdbuf)
{
    if (cmdbuf[0] != IPC_MakeHeader(L256_PLGLDR_GET_DEVICE_LFCS_BLOB, 0, 4)
        || cmdbuf[1] != IPC_Desc_CurProcessId()
        || cmdbuf[3] != IPC_Desc_Buffer(L256_LFCS_BLOB_SIZE, IPC_BUFFER_W))
    {
        error(cmdbuf, 0xD9001830);
        return;
    }

    u8 *out = (u8 *)cmdbuf[4];
    u32 source = L256_LFCS_SOURCE_NONE;
    Result res = L256_AuthorizeLfcsRequest(cmdbuf[2]);

    if (R_SUCCEEDED(res))
        res = L256_ReadLfcsBlob(out, &source);
    else
        memset(out, 0, L256_LFCS_BLOB_SIZE);

    cmdbuf[0] = IPC_MakeHeader(L256_PLGLDR_GET_DEVICE_LFCS_BLOB, 2, 2);
    cmdbuf[1] = (u32)res;
    cmdbuf[2] = source;
    cmdbuf[3] = IPC_Desc_Buffer(L256_LFCS_BLOB_SIZE, IPC_BUFFER_W);
    cmdbuf[4] = (u32)out;
}

void L256_HandleGetRandomBytes(u32 *cmdbuf)
{
    u32 size = cmdbuf[1];

    if (cmdbuf[0] != IPC_MakeHeader(L256_PLGLDR_GET_RANDOM_BYTES, 1, 2)
        || size == 0 || size > L256_RANDOM_BYTES_MAX
        || cmdbuf[2] != IPC_Desc_Buffer(size, IPC_BUFFER_W))
    {
        error(cmdbuf, 0xD9001830);
        return;
    }

    u8 *out = (u8 *)cmdbuf[3];
    Result res = L256_GetRandomBytes(out, size);

    cmdbuf[0] = IPC_MakeHeader(L256_PLGLDR_GET_RANDOM_BYTES, 1, 2);
    cmdbuf[1] = (u32)res;
    cmdbuf[2] = IPC_Desc_Buffer(size, IPC_BUFFER_W);
    cmdbuf[3] = (u32)out;
}
