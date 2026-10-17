#include <3ds.h>
#include <string.h>
#include "csvc.h"
#include "ifile.h"
#include "l256_verify.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"

#include "../../../global/level256/config.h"
#include "../../../global/level256/release_pubkey.h"

#define MANIFEST_SIZE               168
#define MANIFEST_SIGNED_SIZE        104
#define MANIFEST_FORMAT_VERSION     1
#define MANIFEST_ARTIFACT_3GX       1

#define OFF_FORMAT_VERSION          8
#define OFF_ARTIFACT_TYPE           12
#define OFF_IMAGE_LEN               68
#define OFF_IMAGE_HASH              72
#define OFF_SIGNATURE               104

#define IMAGE_LEN_MAX               0x200000

#define PLUGIN_BASE                 0x07000000
#define PLUGIN_HEADER_SIZE          0x100

static const u8 manifestMagic[8] = { 'L', '2', '5', '6', 'M', 'A', 'N', 0x01 };

void l256_die(u32 code, u32 detail, Result rc) __attribute__((noreturn));

__asm__(
    "    .section .text.l256_die, \"ax\", %progbits\n"
    "    .arm\n"
    "    .align 2\n"
    "    .global l256_die\n"
    "    .type l256_die, %function\n"
    "l256_die:\n"
    "    mov r3, r2\n"
    "    mov r2, r1\n"
    "    mov r1, r0\n"
    "    mov r0, #0x4C000000\n"
    "    orr r0, r0, #0x00320000\n"
    "    orr r0, r0, #0x00003500\n"
    "    orr r0, r0, #0x00000036\n"
    "    svc 0x3C\n"
    "    b .\n"
    "    .size l256_die, . - l256_die\n"
);

static inline u32 readLe32(const u8 *p)
{
    u32 v;
    memcpy(&v, p, sizeof(v));
    return v;
}

void l256_verify_plugin(Handle targetProcess)
{
    static u8 manifest[MANIFEST_SIZE];
    static u8 digest[32];

    IFile file;
    u64 fileSize;
    u64 total;
    Result rc;

    rc = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, LEVEL256_PLUGIN_PATH), FS_OPEN_READ);
    if (R_FAILED(rc))
        l256_die(L256_STOP_MANIFEST, L256_DETAIL_OPEN, rc);

    rc = IFile_GetSize(&file, &fileSize);
    if (R_FAILED(rc))
    {
        IFile_Close(&file);
        l256_die(L256_STOP_MANIFEST, L256_DETAIL_GET_SIZE, rc);
    }

    if (fileSize < MANIFEST_SIZE)
    {
        IFile_Close(&file);
        l256_die(L256_STOP_MANIFEST, L256_DETAIL_TOO_SMALL, 0);
    }

    rc = IFile_ReadAt(&file, &total, manifest, (u32)(fileSize - MANIFEST_SIZE), MANIFEST_SIZE);
    IFile_Close(&file);
    if (R_FAILED(rc) || total != MANIFEST_SIZE)
        l256_die(L256_STOP_MANIFEST, L256_DETAIL_READ, R_FAILED(rc) ? rc : 0);

    if (memcmp(manifest, manifestMagic, sizeof(manifestMagic)) != 0)
        l256_die(L256_STOP_MANIFEST, L256_DETAIL_MAGIC, 0);
    if (readLe32(manifest + OFF_FORMAT_VERSION) != MANIFEST_FORMAT_VERSION)
        l256_die(L256_STOP_MANIFEST, L256_DETAIL_FORMAT_VERSION, 0);
    if (readLe32(manifest + OFF_ARTIFACT_TYPE) != MANIFEST_ARTIFACT_3GX)
        l256_die(L256_STOP_MANIFEST, L256_DETAIL_ARTIFACT_TYPE, 0);

    if (crypto_ed25519_check(manifest + OFF_SIGNATURE, LEVEL256_RELEASE_PUBKEY, manifest, MANIFEST_SIGNED_SIZE) != 0)
        l256_die(L256_STOP_SIGNATURE, L256_DETAIL_ED25519, 0);

    u32 imageLen = readLe32(manifest + OFF_IMAGE_LEN);
    if (imageLen == 0 || imageLen > IMAGE_LEN_MAX)
        l256_die(L256_STOP_MANIFEST, L256_DETAIL_IMAGE_LEN, 0);

    u32 mapSize = (PLUGIN_HEADER_SIZE + imageLen + 0xFFF) & ~0xFFF;
    rc = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, PLUGIN_BASE, targetProcess, PLUGIN_BASE, mapSize, 0);
    if (R_FAILED(rc))
        l256_die(L256_STOP_IMAGE, L256_DETAIL_MAP, rc);

    crypto_blake2b(digest, sizeof(digest), (const u8 *)(PLUGIN_BASE + PLUGIN_HEADER_SIZE), imageLen);
    bool match = crypto_verify32(digest, manifest + OFF_IMAGE_HASH) == 0;

    rc = svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, PLUGIN_BASE, mapSize);

    if (!match)
        l256_die(L256_STOP_IMAGE, L256_DETAIL_HASH_MISMATCH, 0);
    if (R_FAILED(rc))
        l256_die(L256_STOP_IMAGE, L256_DETAIL_UNMAP, rc);
}
