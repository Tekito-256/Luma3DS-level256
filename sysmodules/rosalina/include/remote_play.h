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

#define REMOTEPLAY_CONTROL_PORT 8000
#define REMOTEPLAY_VIDEO_PORT   8001
#define REMOTEPLAY_AUDIO_PORT   8002

typedef enum RemotePlayState
{
    REMOTEPLAY_OFF = 0,
    REMOTEPLAY_WAITING,
    REMOTEPLAY_STREAMING,
} RemotePlayState;

typedef struct RemotePlayStatus
{
    RemotePlayState state;
    bool noNetwork;
    u32 viewerIp;
    u32 quality;
    u32 qosBytesPerSecond;
    bool topPriority;
    u32 priorityFactor;
    u32 framesTop, framesBottom;
    bool fromSavedViewer;
    u32 clockMhz;
    bool l2Cache;
    u32 lastFrameBytes;
    u32 copyUs;
    u32 encodeUs;
    u32 sendUs;
    u32 packets;
    u32 sendtoUs;
    u32 sleepUs;
    u32 skippedFrames;
    u32 sendErrors;
    bool lastFromFcram;
    u32 sameFrames;
    u32 tornCopies;
    u32 fps;
    u32 fpsMin, fpsMax;
    u32 busyPct, waitSendPct, waitGamePct;
    u32 encodeMaxUs;
    u32 audioPlayerIp;
    u32 audioFrames;
    u32 audioMissed;
    u32 audioSendErrors;
} RemotePlayStatus;

bool RemotePlay_IsRunning(void);

Result RemotePlay_Start(void);

Result RemotePlay_Stop(void);

void RemotePlay_GetStatus(RemotePlayStatus *out);

void RemotePlay_SaveViewer(void);
