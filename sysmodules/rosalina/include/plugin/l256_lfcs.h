#pragma once

#include <3ds/types.h>

#include "plugin/plgldr.h"

#define L256_LFCS_REQUESTS_PER_LOAD         8

#define L256_LFCS_SOURCE_NONE               0
#define L256_LFCS_SOURCE_B                  1
#define L256_LFCS_SOURCE_A                  2

#define L256_RESULT_NOT_AUTHORIZED          MAKERESULT(RL_PERMANENT, RS_WRONGARG, RM_LDR, RD_NOT_AUTHORIZED)
#define L256_RESULT_RATE_LIMITED            MAKERESULT(RL_PERMANENT, RS_OUTOFRESOURCE, RM_LDR, RD_OUT_OF_RANGE)
#define L256_RESULT_LFCS_NOT_FOUND          MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_LDR, RD_NOT_FOUND)

Result  L256_ReadLfcsBlob(u8 *out, u32 *source);

Result  L256_GetRandomBytes(u8 *out, u32 size);

void    L256_HandleGetDeviceLfcsBlob(u32 *cmdbuf);
void    L256_HandleGetRandomBytes(u32 *cmdbuf);
