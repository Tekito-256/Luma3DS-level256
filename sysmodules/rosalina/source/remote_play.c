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
#include <arpa/inet.h>
#include <string.h>
#include "csvc.h"
#include "draw.h"
#include "fmt.h"
#include "ifile.h"
#include "menu.h"
#include "minisoc.h"
#include "MyThread.h"
#include "remote_play.h"
#include "rp_jpeg.h"
#include "rp_kcopy.h"
#include "sleep.h"
#include "sock_util.h"
#include "utils.h"

extern bool preTerminationRequested;

#define NTR_MAGIC          0x12345678
#define NTR_HEADER_SIZE    84
#define NTR_CMD_HEARTBEAT  0
#define NTR_CMD_REMOTEPLAY 901
#define NTR_MAX_EXDATA     (1u << 20)

#define VIDEO_PACKET_SIZE  1448
#define VIDEO_HEADER_SIZE  4

#define QOS_PER_MBPS       (128 * 1024)
#define QOS_DEFAULT        (8 * QOS_PER_MBPS)
#define QOS_MIN            (500 * 1024)
#define QOS_MAX            (32 * QOS_PER_MBPS)
#define QUALITY_DEFAULT    75

#define MIN_SCREEN_INTERVAL_US 16666
#define LOOK_INTERVAL_US   20000
#define FRAME_POLL_NS      (1000 * 1000LL)
#define SAME_FRAME_RESEND_US 500000
#define MAX_COPY_RETRIES   2
#define HASH_SEED          0x811C9DC5u

#define VIDEO_THREAD_CORE  3
#define ENCODER_THREAD_PRIO 0x30
#define SENDER_THREAD_PRIO 0x2F
#define CONTROL_THREAD_PRIO 0x20
#define SLOT_WAIT_NS       (10 * 1000 * 1000LL)
#define CONTROL_POLL_INTERVAL_MS 10
#define LINK_CHECK_INTERVAL_US 1000000
#define LINK_DOWN_SLEEP_NS (100 * 1000 * 1000LL)

#define FPS_WINDOW_SECONDS 10
#define HISTORY_SECONDS    (FPS_WINDOW_SECONDS + 1)

#define BUFFER_ADDR        0x0E000000
#define JPEG_SLOT_CAP      (128 * 1024)
#define JPEG_SLOTS         2
#define CAPTURE_CAP        (400 * 240 * 4)
#define CONTROL_STACK_SIZE 0x2000
#define ENCODER_STACK_SIZE 0x3000
#define SENDER_STACK_SIZE  0x2000

#define VRAM_PA            0x18000000
#define VRAM_END           0x18600000
#define VRAM_VA            0x1F000000
#define FCRAM_PA           0x20000000
#define FCRAM_END          0x30000000

#define TICKS_PER_US       (SYSCLOCK_ARM11 / 1000000)

#define N3DS_CPU_FAST      3

#define SAVED_VIEWER_PATH  "/luma/streaming.bin"
#define SAVED_VIEWER_MAGIC 0x31505232

typedef struct SavedViewer
{
    u32 magic;
    u32 ip;
    u32 mode;
    u32 quality;
    u32 qos;
} SavedViewer;

#define KERNPA2VA(a)       ((a) + (GET_VERSION_MINOR(osGetKernelVersion()) < 44 ? 0xD0000000 : 0xC0000000))

static MyThread controlThread, encoderThread, senderThread;

static volatile bool running, stopRequested;
static volatile bool linkDown, checkLinkNow;
static bool socInitialized;
static int listenSock = -1, videoSock = -1;

static LightLock settingsLock;
static bool settingsLockReady;
static u32 settingsGeneration;
static u32 viewerIp;
static u32 settingMode, settingQuality, settingQos;

static RemotePlayStatus status;

static SavedViewer savedViewer;
static bool savedViewerRead;

static s64 clockStateBefore = -1;

static inline u32 ticksToUs(u64 ticks)
{
    return (u32)(ticks / TICKS_PER_US);
}

static void initSettingsLock(void)
{
    if (!settingsLockReady)
    {
        LightLock_Init(&settingsLock);
        settingsLockReady = true;
    }
}

bool RemotePlay_IsRunning(void)
{
    return running;
}

typedef struct NtrHeader
{
    u32 magic;
    u32 seq;
    u32 type;
    u32 cmd;
    u32 args[16];
    u32 exdataLen;
} NtrHeader;

_Static_assert(sizeof(NtrHeader) == NTR_HEADER_SIZE, "NTR header is 84 bytes");

typedef struct ControlConn
{
    int sock;
    u32 peerIp;
    u8 header[NTR_HEADER_SIZE];
    u32 headerGot;
    u32 exdataLeft;
} ControlConn;

static void closeSocket(int *sock)
{
    if (*sock < 0)
        return;

    struct linger linger = { .l_onoff = 1, .l_linger = 0 };
    socSetsockopt(*sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
    socClose(*sock);
    *sock = -1;
}

static int openListenSocket(void)
{
    u32 ip = socGethostid();
    if (ip == 0 || ip == (u32)-1)
        return -1;

    int sock = socSocket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return sock;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(REMOTEPLAY_CONTROL_PORT),
        .sin_addr.s_addr = ip,
    };
    int res = socBind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (res == 0)
        res = socListen(sock, 1);
    if (res != 0)
    {
        closeSocket(&sock);
        return res < 0 ? res : -1;
    }
    return sock;
}

static void applySettings(u32 ip, u32 mode, u32 quality, u32 qos, bool fromSaved)
{
    if (quality == 0 || quality > 100)
        quality = QUALITY_DEFAULT;
    if (qos < QOS_MIN)
        qos = QOS_DEFAULT;
    else if (qos > QOS_MAX)
        qos = QOS_MAX;

    LightLock_Lock(&settingsLock);
    viewerIp = ip;
    settingMode = mode;
    settingQuality = quality;
    settingQos = qos;
    settingsGeneration++;

    status.state = REMOTEPLAY_STREAMING;
    status.viewerIp = ip;
    status.quality = quality;
    status.qosBytesPerSecond = qos;
    status.topPriority = ((mode >> 8) & 0xFF) != 0;
    status.priorityFactor = mode & 0xFF;
    status.fromSavedViewer = fromSaved;
    LightLock_Unlock(&settingsLock);
}

static void handleRemotePlayRequest(const NtrHeader *h, u32 peerIp)
{
    applySettings(peerIp, h->args[0], h->args[1], h->args[2], false);
}

static void readSavedViewer(void)
{
    if (savedViewerRead)
        return;
    savedViewerRead = true;

    IFile file;
    u64 total = 0;
    SavedViewer v;
    if (R_FAILED(IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, SAVED_VIEWER_PATH), FS_OPEN_READ)))
        return;
    Result res = IFile_Read(&file, &total, &v, sizeof(v));
    IFile_Close(&file);

    if (R_SUCCEEDED(res) && total == sizeof(v) && v.magic == SAVED_VIEWER_MAGIC && v.ip != 0)
        savedViewer = v;
}

void RemotePlay_SaveViewer(void)
{
    initSettingsLock();
    LightLock_Lock(&settingsLock);
    SavedViewer v = {
        .magic = SAVED_VIEWER_MAGIC,
        .ip = viewerIp,
        .mode = settingMode,
        .quality = settingQuality,
        .qos = settingQos,
    };
    LightLock_Unlock(&settingsLock);

    readSavedViewer();
    if (v.ip == 0 || memcmp(&v, &savedViewer, sizeof(v)) == 0)
        return;

    IFile file;
    u64 total = 0;
    Result res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, SAVED_VIEWER_PATH), FS_OPEN_CREATE | FS_OPEN_WRITE);
    if (R_SUCCEEDED(res))
    {
        res = IFile_SetSize(&file, sizeof(v));
        if (R_SUCCEEDED(res))
            res = IFile_Write(&file, &total, &v, sizeof(v), 0);
        IFile_Close(&file);
    }

    savedViewer = v;
}

static bool cpuIsFast(void)
{
    s64 clk = 0, l2 = 0;
    svcGetSystemInfo(&clk, 0x10001, 0);
    svcGetSystemInfo(&l2, 0x10001, 2);
    status.clockMhz = (u32)clk;
    status.l2Cache = l2 != 0;
    return clk != 268 && l2 != 0;
}

static void raiseClock(void)
{
    if (!cpuIsFast())
    {
        svcKernelSetState(10, N3DS_CPU_FAST);
        cpuIsFast();
    }
}

static bool sendAll(int sock, const void *data, u32 size)
{
    const u8 *p = (const u8 *)data;
    while (size != 0)
    {
        int n = socSend(sock, p, size, 0);
        if (n <= 0)
            return false;
        p += n;
        size -= n;
    }
    return true;
}

static bool handlePacket(ControlConn *c)
{
    NtrHeader h;
    memcpy(&h, c->header, sizeof(h));

    if (h.magic != NTR_MAGIC || h.exdataLen > NTR_MAX_EXDATA)
        return false;

    c->exdataLeft = h.exdataLen;

    switch (h.cmd)
    {
        case NTR_CMD_HEARTBEAT:
        {
            NtrHeader reply = { .magic = NTR_MAGIC, .seq = h.seq, .type = 0, .cmd = NTR_CMD_HEARTBEAT };
            return sendAll(c->sock, &reply, sizeof(reply));
        }
        case NTR_CMD_REMOTEPLAY:
            handleRemotePlayRequest(&h, c->peerIp);
            return true;
        default:
            return true;
    }
}

static bool readFromConn(ControlConn *c)
{
    u8 scratch[256];
    int n;

    if (c->exdataLeft != 0)
    {
        u32 want = c->exdataLeft < sizeof(scratch) ? c->exdataLeft : sizeof(scratch);
        n = socRecv(c->sock, scratch, want, 0);
        if (n <= 0)
            return false;
        c->exdataLeft -= n;
        return true;
    }

    n = socRecv(c->sock, c->header + c->headerGot, NTR_HEADER_SIZE - c->headerGot, 0);
    if (n <= 0)
        return false;

    c->headerGot += n;
    if (c->headerGot < NTR_HEADER_SIZE)
        return true;

    c->headerGot = 0;
    return handlePacket(c);
}

static void checkLink(ControlConn *conn)
{
    checkLinkNow = false;
    bool down = !Wifi__IsConnected();

    if (down && !linkDown)
    {
        closeSocket(&conn->sock);
        closeSocket(&listenSock);
    }
    linkDown = down;

    raiseClock();
}

static void controlThreadMain(void)
{
    ControlConn conn = { .sock = -1 };
    u64 lastLinkCheck = 0;

    while (!stopRequested && !preTerminationRequested)
    {
        if (Sleep__Status())
            checkLinkNow = true;

        u64 now = svcGetSystemTick();
        if (checkLinkNow || now - lastLinkCheck >= (u64)LINK_CHECK_INTERVAL_US * TICKS_PER_US)
        {
            lastLinkCheck = now;
            checkLink(&conn);
        }

        if (linkDown)
        {
            svcSleepThread(CONTROL_POLL_INTERVAL_MS * 1000 * 1000LL);
            continue;
        }

        if (listenSock < 0)
        {
            svcSleepThread(1000 * 1000 * 1000LL);
            listenSock = openListenSocket();
            continue;
        }

        struct pollfd fds[2] = {
            { .fd = listenSock, .events = POLLIN },
            { .fd = conn.sock, .events = POLLIN },
        };
        nfds_t nfds = conn.sock >= 0 ? 2 : 1;

        int res = socPoll(fds, nfds, 0);
        if (res < -10000)
            break;
        if (res <= 0)
        {
            svcSleepThread(CONTROL_POLL_INTERVAL_MS * 1000 * 1000LL);
            continue;
        }

        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            closeSocket(&listenSock);
            continue;
        }

        if (fds[0].revents & POLLIN)
        {
            struct sockaddr_in addr;
            socklen_t addrLen = sizeof(addr);
            int sock = socAccept(listenSock, (struct sockaddr *)&addr, &addrLen);
            if (sock >= 0)
            {
                closeSocket(&conn.sock);
                memset(&conn, 0, sizeof(conn));
                conn.sock = sock;
                conn.peerIp = addr.sin_addr.s_addr;
            }
            else
                closeSocket(&listenSock);
        }
        else if (nfds == 2 && (fds[1].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)))
        {
            if (!readFromConn(&conn))
                closeSocket(&conn.sock);
        }
    }

    closeSocket(&conn.sock);
}

typedef struct JpegSlot
{
    volatile u32 size;
    bool top;
    u8 frameId;
    u32 destIp;
    u64 packetIntervalTicks;
    u8 data[JPEG_SLOT_CAP];
} JpegSlot;

typedef struct ScreenState
{
    u64 lastFrameTick;
    u64 lastLookTick;
    u32 lastPa;
    u32 lastHash;
    bool haveHash;
    u8 frameId;
} ScreenState;

typedef struct EncoderState
{
    RpJpeg jpeg;
    u32 generation;
    u32 destIp;
    bool topPriority;
    u32 priorityFactor;
    u64 packetIntervalTicks;
    ScreenState screens[2];
    u32 lowCredit;
    u32 slot;
} EncoderState;

typedef struct SenderState
{
    u64 lastPacketTick;
    u32 slot;
    u8 packet[VIDEO_PACKET_SIZE];
} SenderState;

enum { ENC_BUSY, ENC_WAIT_SEND, ENC_WAIT_GAME, ENC_TIME_KINDS };

typedef struct EncSecond
{
    u32 second;
    u32 us[ENC_TIME_KINDS];
    u32 encodeMaxUs;
} EncSecond;

typedef struct SndSecond
{
    u32 second;
    u16 framesTop, framesBottom;
} SndSecond;

typedef struct Buffers
{
    JpegSlot slots[JPEG_SLOTS];
    u8 capture[CAPTURE_CAP];
    u8 CTR_ALIGN(8) controlStack[CONTROL_STACK_SIZE];
    u8 CTR_ALIGN(8) encoderStack[ENCODER_STACK_SIZE];
    u8 CTR_ALIGN(8) senderStack[SENDER_STACK_SIZE];
    EncoderState encoder;
    SenderState sender;
    u64 startTick;
    EncSecond encHistory[HISTORY_SECONDS];
    SndSecond sndHistory[HISTORY_SECONDS];
} Buffers;

#define BUFFER_SIZE ((sizeof(Buffers) + 0xFFF) & ~0xFFF)

static Buffers *buffers;
#define enc (buffers->encoder)
#define snd (buffers->sender)

static u32 secondOf(u64 tick)
{
    return (u32)((tick - buffers->startTick) / SYSCLOCK_ARM11) + 1;
}

static u64 startOfSecond(u32 second)
{
    return buffers->startTick + (u64)(second - 1) * SYSCLOCK_ARM11;
}

static void *historyEntry(void *ring, u32 entrySize, u32 second)
{
    u32 *entry = (u32 *)((u8 *)ring + (second % HISTORY_SECONDS) * entrySize);
    if (*entry != second)
    {
        memset(entry, 0, entrySize);
        *entry = second;
    }
    return entry;
}

static EncSecond *encEntry(u32 second)
{
    return historyEntry(buffers->encHistory, sizeof(EncSecond), second);
}

static SndSecond *sndEntry(u32 second)
{
    return historyEntry(buffers->sndHistory, sizeof(SndSecond), second);
}

static inline u32 maxU32(u32 a, u32 b)
{
    return a > b ? a : b;
}

static void encoderSpent(u32 kind, u64 from, u64 to)
{
    u64 oldest = to > (u64)(HISTORY_SECONDS - 1) * SYSCLOCK_ARM11 ? to - (u64)(HISTORY_SECONDS - 1) * SYSCLOCK_ARM11 : 0;
    if (from < oldest)
        from = oldest;
    if (from < buffers->startTick)
        from = buffers->startTick;

    while (from < to)
    {
        u32 s = secondOf(from);
        u64 end = startOfSecond(s + 1);
        if (end > to)
            end = to;
        encEntry(s)->us[kind] += ticksToUs(end - from);
        from = end;
    }
}

static u32 framesIn(u32 s)
{
    const SndSecond *n = &buffers->sndHistory[s % HISTORY_SECONDS];
    return n->second == s ? n->framesTop + n->framesBottom : 0;
}

void RemotePlay_GetStatus(RemotePlayStatus *out)
{
    initSettingsLock();
    LightLock_Lock(&settingsLock);
    *out = status;
    LightLock_Unlock(&settingsLock);

    out->noNetwork = running && linkDown;
    out->fps = out->fpsMin = out->fpsMax = 0;
    out->busyPct = out->waitSendPct = out->waitGamePct = out->encodeMaxUs = 0;
    if (!running || buffers == NULL)
        return;

    u32 now = secondOf(svcGetSystemTick());
    if (now <= 1)
        return;

    const EncSecond *e = &buffers->encHistory[(now - 1) % HISTORY_SECONDS];
    if (e->second == now - 1)
    {
        out->busyPct = e->us[ENC_BUSY] / 10000;
        out->waitSendPct = e->us[ENC_WAIT_SEND] / 10000;
        out->waitGamePct = e->us[ENC_WAIT_GAME] / 10000;
        out->encodeMaxUs = e->encodeMaxUs;
    }

    out->fps = framesIn(now - 1);
    out->fpsMin = ~0u;
    for (u32 s = now - 1; s >= 1 && now - s <= FPS_WINDOW_SECONDS; s--)
    {
        u32 fps = framesIn(s);
        out->fpsMin = fps < out->fpsMin ? fps : out->fpsMin;
        out->fpsMax = maxU32(out->fpsMax, fps);
    }
}

static LightEvent slotFilled, slotFreed;

static u32 currentFramebufferPa(bool top)
{
    if (top)
        return (GPU_FB_TOP_SEL & 1) ? GPU_FB_TOP_LEFT_ADDR_2 : GPU_FB_TOP_LEFT_ADDR_1;
    else
        return (GPU_FB_BOTTOM_SEL & 1) ? GPU_FB_BOTTOM_ADDR_2 : GPU_FB_BOTTOM_ADDR_1;
}

typedef struct Capture
{
    const u8 *pixels;
    u32 pitch;
    u32 rows;
    int format;
    u32 pa;
    u32 hash;
} Capture;

static bool captureOnce(bool top, Capture *c)
{
    u32 formatReg = top ? GPU_FB_TOP_FMT : GPU_FB_BOTTOM_FMT;
    int format = (int)(formatReg & 7);
    u32 bpp = RpJpeg_BytesPerPixel(format);
    u32 stride = top ? GPU_FB_TOP_STRIDE : GPU_FB_BOTTOM_STRIDE;
    u32 pa = currentFramebufferPa(top);
    u32 rowBytes = 240 * bpp;

    c->pa = pa;
    if (bpp == 0 || stride < rowBytes || stride > 0x1000)
        return false;

    u32 memRows = 320, rowStep = 1;
    if (top)
    {
        bool is3d = (formatReg & BIT(5)) != 0;
        bool isNormal2d = (formatReg & BIT(6)) != 0;
        memRows = !is3d && !isNormal2d ? 800 : 400;
        rowStep = memRows / 400;
    }

    u32 rows = memRows / rowStep;
    u32 span = (memRows - 1) * stride + rowBytes;
    u8 *snapshot = buffers->capture;

    c->rows = rows;
    c->format = format;
    c->pixels = snapshot;
    c->pitch = rowBytes;
    c->hash = HASH_SEED ^ (u32)format ^ (rows << 8);

    if (pa >= VRAM_PA && pa < VRAM_END && span <= VRAM_END - pa)
    {
        u8 *src = (u8 *)(VRAM_VA + (pa - VRAM_PA));
        svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)src, span);
        status.lastFromFcram = false;
        return RpCopy_Rows(snapshot, src, rows, stride, rowStep, rowBytes, &c->hash);
    }

    if (pa >= FCRAM_PA && pa < FCRAM_END && span <= FCRAM_END - pa)
    {
        svcFlushEntireDataCache();
        status.lastFromFcram = true;
        return RpKernelCopy_Rows(snapshot, KERNPA2VA(pa), rows, stride, rowStep, rowBytes, &c->hash);
    }

    return false;
}

static bool captureScreen(bool top, Capture *c)
{
    for (u32 attempt = 0; ; attempt++)
    {
        if (!captureOnce(top, c))
            return false;

        if (currentFramebufferPa(top) == c->pa || attempt == MAX_COPY_RETRIES)
            return true;

        status.tornCopies++;
    }
}

static bool sendFrame(const JpegSlot *slot)
{
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(REMOTEPLAY_VIDEO_PORT),
        .sin_addr.s_addr = slot->destIp,
    };

    const u8 *jpeg = slot->data;
    u32 size = slot->size;
    bool top = slot->top;
    u8 *pkt = snd.packet;
    pkt[0] = slot->frameId;
    pkt[2] = 2;
    pkt[3] = 0;

    u32 packets = 0;
    u64 sendtoTicks = 0, sleepTicks = 0;

    while (size != 0 && !stopRequested && !linkDown)
    {
        u32 chunk = size < VIDEO_PACKET_SIZE - VIDEO_HEADER_SIZE ? size : VIDEO_PACKET_SIZE - VIDEO_HEADER_SIZE;
        size -= chunk;
        pkt[1] = (top ? 1 : 0) | (size == 0 ? 0x10 : 0);
        memcpy(pkt + VIDEO_HEADER_SIZE, jpeg, chunk);
        jpeg += chunk;

        u64 now = svcGetSystemTick();
        u64 elapsed = now - snd.lastPacketTick;
        if (elapsed < slot->packetIntervalTicks)
        {
            svcSleepThread((s64)ticksToUs(slot->packetIntervalTicks - elapsed) * 1000);
            u64 woke = svcGetSystemTick();
            sleepTicks += woke - now;
            now = woke;
        }

        int res = socSendto(videoSock, pkt, VIDEO_HEADER_SIZE + chunk, 0, (struct sockaddr *)&addr, sizeof(addr));
        snd.lastPacketTick = svcGetSystemTick();
        if (res < 0)
        {
            status.sendErrors++;
            return false;
        }

        sendtoTicks += snd.lastPacketTick - now;
        packets++;
        pkt[3]++;
    }

    status.packets = packets;
    status.sendtoUs = ticksToUs(sendtoTicks);
    status.sleepUs = ticksToUs(sleepTicks);
    return true;
}

static bool takeSettings(void)
{
    LightLock_Lock(&settingsLock);
    u32 generation = settingsGeneration;
    u32 ip = viewerIp, mode = settingMode, quality = settingQuality, qos = settingQos;
    LightLock_Unlock(&settingsLock);

    if (ip == 0)
        return false;

    if (generation != enc.generation)
    {
        enc.generation = generation;
        enc.destIp = ip;
        enc.topPriority = ((mode >> 8) & 0xFF) != 0;
        enc.priorityFactor = mode & 0xFF;
        enc.packetIntervalTicks = (u64)SYSCLOCK_ARM11 * VIDEO_PACKET_SIZE / qos;
        RpJpeg_SetQuality(&enc.jpeg, (int)quality);
    }
    return true;
}

static bool waitForSlot(const JpegSlot *slot, bool filled, LightEvent *event)
{
    while ((slot->size != 0) != filled)
    {
        if (stopRequested || preTerminationRequested)
            return false;
        LightEvent_WaitTimeout(event, SLOT_WAIT_NS);
    }
    __dmb();
    return true;
}

static bool screenReady(bool top, u64 now)
{
    const ScreenState *sc = &enc.screens[top];

    if (now - sc->lastFrameTick < (u64)MIN_SCREEN_INTERVAL_US * TICKS_PER_US)
        return false;
    return currentFramebufferPa(top) != sc->lastPa || now - sc->lastLookTick >= (u64)LOOK_INTERVAL_US * TICKS_PER_US;
}

static int pickScreen(void)
{
    bool prio = enc.topPriority;

    while (!stopRequested && !preTerminationRequested)
    {
        u64 now = svcGetSystemTick();
        if (enc.priorityFactor != 0 && enc.lowCredit >= enc.priorityFactor && screenReady(!prio, now))
            return !prio;
        if (screenReady(prio, now))
            return prio;
        svcSleepThread(FRAME_POLL_NS);
    }
    return -1;
}

static void encoderThreadMain(void)
{
    while (!stopRequested && !preTerminationRequested)
    {
        if (Sleep__Status())
            checkLinkNow = true;

        if (linkDown)
        {
            svcSleepThread(LINK_DOWN_SLEEP_NS);
            continue;
        }

        if (!takeSettings())
        {
            svcSleepThread(50 * 1000 * 1000LL);
            continue;
        }

        JpegSlot *slot = &buffers->slots[enc.slot];
        u64 waitStart = svcGetSystemTick();
        if (!waitForSlot(slot, false, &slotFreed))
            break;
        u64 slotFree = svcGetSystemTick();
        int screen = pickScreen();
        if (screen < 0)
            break;
        u64 frameStart = svcGetSystemTick();
        encoderSpent(ENC_WAIT_SEND, waitStart, slotFree);
        encoderSpent(ENC_WAIT_GAME, slotFree, frameStart);

        bool top = screen != 0;
        ScreenState *sc = &enc.screens[top];
        enc.lowCredit = top != enc.topPriority ? 0 : enc.lowCredit + 1;

        Capture c;
        bool readable = captureScreen(top, &c);
        u64 copied = svcGetSystemTick();
        sc->lastLookTick = frameStart;
        sc->lastPa = c.pa;

        if (!readable)
        {
            encoderSpent(ENC_BUSY, frameStart, copied);
            status.skippedFrames++;
            svcSleepThread(5 * 1000 * 1000LL);
            continue;
        }

        if (sc->haveHash && c.hash == sc->lastHash
            && frameStart - sc->lastFrameTick < (u64)SAME_FRAME_RESEND_US * TICKS_PER_US)
        {
            encoderSpent(ENC_BUSY, frameStart, copied);
            status.sameFrames++;
            continue;
        }

        u32 size = RpJpeg_Encode(&enc.jpeg, slot->data, JPEG_SLOT_CAP, c.pixels, 240, c.rows, c.pitch, 1, c.format);
        u64 encoded = svcGetSystemTick();
        encoderSpent(ENC_BUSY, frameStart, encoded);

        if (size == 0)
        {
            status.skippedFrames++;
            svcSleepThread(5 * 1000 * 1000LL);
            continue;
        }

        sc->lastFrameTick = frameStart;
        sc->lastHash = c.hash;
        sc->haveHash = true;

        if (++sc->frameId == 0)
            sc->frameId = 1;

        status.copyUs = ticksToUs(copied - frameStart);
        status.encodeUs = ticksToUs(encoded - copied);
        EncSecond *e = encEntry(secondOf(encoded));
        e->encodeMaxUs = maxU32(e->encodeMaxUs, ticksToUs(encoded - frameStart));

        slot->top = top;
        slot->frameId = sc->frameId;
        slot->destIp = enc.destIp;
        slot->packetIntervalTicks = enc.packetIntervalTicks;
        __dmb();
        slot->size = size;
        LightEvent_Signal(&slotFilled);

        enc.slot = (enc.slot + 1) % JPEG_SLOTS;
    }
}

static void senderThreadMain(void)
{
    while (!stopRequested && !preTerminationRequested)
    {
        JpegSlot *slot = &buffers->slots[snd.slot];
        if (!waitForSlot(slot, true, &slotFilled))
            break;

        if (Sleep__Status())
            checkLinkNow = true;

        u64 start = svcGetSystemTick();
        bool ok = linkDown || sendFrame(slot);
        u64 sent = svcGetSystemTick();
        u32 size = slot->size;
        bool top = slot->top;

        __dmb();
        slot->size = 0;
        LightEvent_Signal(&slotFreed);
        snd.slot = (snd.slot + 1) % JPEG_SLOTS;

        if (!ok)
        {
            checkLinkNow = true;
            closeSocket(&videoSock);
            videoSock = socSocket(AF_INET, SOCK_DGRAM, 0);
            svcSleepThread(100 * 1000 * 1000LL);
            continue;
        }
        if (linkDown)
            continue;

        status.lastFrameBytes = size;
        status.sendUs = ticksToUs(sent - start);
        if (top)
            status.framesTop++;
        else
            status.framesBottom++;

        SndSecond *e = sndEntry(secondOf(sent));
        if (top)
            e->framesTop++;
        else
            e->framesBottom++;
    }
}

static void freeEverything(void)
{
    if (clockStateBefore >= 0)
    {
        svcKernelSetState(10, (u32)clockStateBefore);
        clockStateBefore = -1;
    }

    closeSocket(&listenSock);
    closeSocket(&videoSock);

    if (socInitialized)
    {
        miniSocExit();
        socInitialized = false;
    }

    if (buffers != NULL)
    {
        u32 tmp;
        svcControlMemory(&tmp, (u32)buffers, 0, BUFFER_SIZE, MEMOP_FREE, 0);
        buffers = NULL;
    }
}

Result RemotePlay_Start(void)
{
    Result res;
    u32 tmp;

    if (running)
        return 0;
    if (!isN3DS)
        return -1;

    initSettingsLock();

    stopRequested = false;
    LightLock_Lock(&settingsLock);
    memset(&status, 0, sizeof(status));
    status.state = REMOTEPLAY_WAITING;
    viewerIp = 0;
    settingsGeneration++;
    LightLock_Unlock(&settingsLock);

    readSavedViewer();

    res = svcControlMemoryEx(&tmp, BUFFER_ADDR, 0, BUFFER_SIZE, MEMOP_ALLOC | MEMOP_REGION_SYSTEM, MEMPERM_READWRITE, true);
    if (R_FAILED(res))
        goto fail;
    buffers = (Buffers *)BUFFER_ADDR;
    memset(&enc, 0, sizeof(enc));
    memset(&snd, 0, sizeof(snd));
    memset(buffers->encHistory, 0, sizeof(buffers->encHistory));
    memset(buffers->sndHistory, 0, sizeof(buffers->sndHistory));
    buffers->startTick = svcGetSystemTick();
    linkDown = checkLinkNow = false;
    for (u32 i = 0; i < JPEG_SLOTS; i++)
        buffers->slots[i].size = 0;
    LightEvent_Init(&slotFilled, RESET_ONESHOT);
    LightEvent_Init(&slotFreed, RESET_ONESHOT);

    res = miniSocInitEx(false);
    if (R_FAILED(res))
        goto fail;
    socInitialized = true;

    listenSock = openListenSocket();
    videoSock = socSocket(AF_INET, SOCK_DGRAM, 0);
    if (listenSock < 0 || videoSock < 0)
    {
        res = listenSock < 0 ? listenSock : videoSock;
        goto fail;
    }

    if (savedViewer.ip != 0)
        applySettings(savedViewer.ip, savedViewer.mode, savedViewer.quality, savedViewer.qos, true);

    {
        s64 clk = 0, l2 = 0;
        svcGetSystemInfo(&clk, 0x10001, 0);
        svcGetSystemInfo(&l2, 0x10001, 2);
        clockStateBefore = (l2 != 0 ? 2 : 0) | (clk != 268 ? 1 : 0);
        raiseClock();
    }

    running = true;
    if (R_FAILED(res = MyThread_Create(&controlThread, controlThreadMain, buffers->controlStack,
                                       CONTROL_STACK_SIZE, CONTROL_THREAD_PRIO, CORE_SYSTEM)))
    {
        running = false;
        goto fail;
    }
    if (R_FAILED(res = MyThread_Create(&senderThread, senderThreadMain, buffers->senderStack,
                                       SENDER_STACK_SIZE, SENDER_THREAD_PRIO, VIDEO_THREAD_CORE)))
    {
        stopRequested = true;
        MyThread_Join(&controlThread, 5 * 1000 * 1000 * 1000LL);
        running = false;
        goto fail;
    }
    if (R_FAILED(res = MyThread_Create(&encoderThread, encoderThreadMain, buffers->encoderStack,
                                       ENCODER_STACK_SIZE, ENCODER_THREAD_PRIO, VIDEO_THREAD_CORE)))
    {
        stopRequested = true;
        MyThread_Join(&controlThread, 5 * 1000 * 1000 * 1000LL);
        MyThread_Join(&senderThread, 5 * 1000 * 1000 * 1000LL);
        running = false;
        goto fail;
    }

    return 0;

fail:
    freeEverything();
    LightLock_Lock(&settingsLock);
    status.state = REMOTEPLAY_OFF;
    viewerIp = 0;
    LightLock_Unlock(&settingsLock);
    return res;
}

Result RemotePlay_Stop(void)
{
    if (!running)
        return 0;

    stopRequested = true;

    MyThread *threads[] = { &controlThread, &senderThread, &encoderThread };
    Result res = 0;
    for (u32 i = 0; i < sizeof(threads) / sizeof(threads[0]); i++)
    {
        Result r = threads[i]->handle != 0 ? MyThread_Join(threads[i], 5 * 1000 * 1000 * 1000LL) : 0;
        if (R_SUCCEEDED(res))
            res = r;
    }
    if (R_FAILED(res))
        return res;

    RemotePlay_SaveViewer();
    freeEverything();
    running = false;

    LightLock_Lock(&settingsLock);
    status.state = REMOTEPLAY_OFF;
    viewerIp = 0;
    LightLock_Unlock(&settingsLock);
    return 0;
}
