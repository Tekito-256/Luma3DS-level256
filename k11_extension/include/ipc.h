/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#pragma once

#include "types.h"
#include "globals.h"
#include "kernel.h"
#include "utils.h"

/// IPC buffer access rights.
typedef enum
{
	IPC_BUFFER_R  = BIT(1),                     ///< Readable
	IPC_BUFFER_W  = BIT(2),                     ///< Writable
	IPC_BUFFER_RW = IPC_BUFFER_R | IPC_BUFFER_W ///< Readable and Writable
} IPC_BufferRights;

static inline u32 IPC_MakeHeader(u16 command_id, unsigned normal_params, unsigned translate_params)
{
	return ((u32) command_id << 16) | (((u32) normal_params & 0x3F) << 6) | (((u32) translate_params & 0x3F) << 0);
}

static inline u32 IPC_Desc_Buffer(size_t size, IPC_BufferRights rights)
{
	return (size << 4) | 0x8 | rights;
}

#define MAX_SESSION     345

// the structure of sessions is apparently not the same on older versions...

typedef struct SessionInfo
{
    KSession *session;
    char name[12];
} SessionInfo;

typedef struct LangemuAttributes
{
    u64 titleId;
    u8 mask, region, language, country, state;
} LangemuAttributes;

extern KRecursiveLock processLangemuLock;
extern LangemuAttributes processLangemuAttributes[0x40];

SessionInfo *SessionInfo_Lookup(KSession *session);
SessionInfo *SessionInfo_FindFirst(const char *name);
void SessionInfo_ChangeVtable(KSession *session);
void SessionInfo_Add(KSession *session, const char *name);
void SessionInfo_Remove(KSession *session);

bool doLangEmu(Result *res, u32 *cmdbuf);
bool doErrfThrowHook(u32 *cmdbuf);
