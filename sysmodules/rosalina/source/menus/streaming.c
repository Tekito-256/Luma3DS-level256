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
#include "draw.h"
#include "fmt.h"
#include "menu.h"
#include "menus/streaming.h"
#include "minisoc.h"
#include "remote_play.h"

static void formatIp(char *buf, u32 ip)
{
    const u8 *a = (const u8 *)&ip;
    sprintf(buf, "%hhu.%hhu.%hhu.%hhu", a[0], a[1], a[2], a[3]);
}

#define MS_1(us) (us) / 1000, (us) / 100 % 10

static void drawStatus(Result lastResult)
{
    RemotePlayStatus st;
    RemotePlay_GetStatus(&st);

    char ip[16];
    u32 posY = 30;

    Draw_DrawString(10, 10, COLOR_TITLE, "Wireless streaming");

    posY = Draw_DrawString(10, posY, COLOR_WHITE,
        "Sends the screens to a PC (Snickerstream or\n"
        "Chokistream, NTR mode). Picture only.\n\n");

    if (st.noNetwork)
    {
        posY = Draw_DrawString(10, posY, COLOR_RED, "Paused: the console is offline.\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE,
            "Local wireless play takes the Wi-Fi away from\n"
            "the internet. Streaming picks up again once\n"
            "the console is back online.\n");
    }
    else switch (st.state)
    {
        case REMOTEPLAY_OFF:
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Status: off\n");
            if (R_FAILED(lastResult))
                posY = Draw_DrawFormattedString(10, posY, COLOR_RED,
                    "Could not start (0x%08lx).\nIs the console connected to Wi-Fi?\n", (u32)lastResult);
            break;

        case REMOTEPLAY_WAITING:
            formatIp(ip, (u32)socGethostid());
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Status: waiting for the viewer\n");
            posY = Draw_DrawFormattedString(10, posY, COLOR_GREEN, "Enter this IP in the viewer: %s\n", ip);
            break;

        case REMOTEPLAY_STREAMING:
            formatIp(ip, (u32)socGethostid());
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Status: streaming   (3DS IP %s)\n", ip);
            formatIp(ip, st.viewerIp);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Viewer: %s%s\n", ip,
                st.fromSavedViewer ? " (last time's)" : "");
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Quality %lu, QoS %lu (%lu KiB/s), %s first\n",
                st.quality, st.qosBytesPerSecond / (128 * 1024), st.qosBytesPerSecond / 1024,
                st.topPriority ? "top" : "bottom");
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "%2lu fps (10 s: %lu-%lu), last frame %lu KB%s\n",
                st.fps, st.fpsMin, st.fpsMax, st.lastFrameBytes / 1024, st.lastFromFcram ? " (FCRAM)" : "");
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Copy %lu.%lu ms, encode %lu ms (max %lu)\n",
                MS_1(st.copyUs), st.encodeUs / 1000, st.encodeMaxUs / 1000);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Send %lu ms: sendto %lu, sleep %lu (%lu pkts)\n",
                st.sendUs / 1000, st.sendtoUs / 1000, st.sleepUs / 1000, st.packets);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Encoder: busy %lu%%, wait send %lu%%, game %lu%%\n",
                st.busyPct, st.waitSendPct, st.waitGamePct);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Frames %lu / %lu, same %lu, torn %lu\n",
                st.framesTop, st.framesBottom, st.sameFrames, st.tornCopies);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Skipped %lu, errors %lu, CPU %lu MHz, L2 %s\n",
                st.skippedFrames, st.sendErrors, st.clockMhz, st.l2Cache ? "on" : "off");
            break;
    }

    posY += SPACING_Y;
    Draw_DrawString(10, posY, COLOR_TITLE,
        RemotePlay_IsRunning() ? "A: stop, B: back." : "A: start, B: back.");
}

void N3DSMenu_Streaming(void)
{
    Result lastResult = 0;

    do
    {
        Draw_Lock();
        Draw_ClearFramebuffer();
        drawStatus(lastResult);
        Draw_FlushFramebuffer();
        Draw_Unlock();

        if (RemotePlay_IsRunning())
            RemotePlay_SaveViewer();

        u32 pressed = waitInputWithTimeout(500);

        if (pressed & KEY_A)
        {
            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_DrawString(10, 10, COLOR_TITLE, "Wireless streaming");
            Draw_DrawString(10, 30, COLOR_WHITE, RemotePlay_IsRunning() ? "Stopping..." : "Starting...");
            Draw_FlushFramebuffer();
            Draw_Unlock();

            lastResult = RemotePlay_IsRunning() ? RemotePlay_Stop() : RemotePlay_Start();
        }
        else if (pressed & KEY_B)
            break;
    }
    while (!menuShouldExit);
}
