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

bool RpCopy_Rows(void *dst, const void *src, u32 rows, u32 stride, u32 rowStep, u32 rowBytes, u32 *hash);

bool RpKernelCopy_Rows(void *dst, u32 kernelVa, u32 rows, u32 stride, u32 rowStep, u32 rowBytes, u32 *hash);
