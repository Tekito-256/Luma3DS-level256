#pragma once

#include <3ds/types.h>

#define L256_STOP_MANIFEST              0x10
#define L256_STOP_SIGNATURE             0x11
#define L256_STOP_IMAGE                 0x12

#define L256_DETAIL_OPEN                1
#define L256_DETAIL_GET_SIZE            2
#define L256_DETAIL_TOO_SMALL           3
#define L256_DETAIL_READ                4
#define L256_DETAIL_MAGIC               5
#define L256_DETAIL_FORMAT_VERSION      6
#define L256_DETAIL_ARTIFACT_TYPE       7
#define L256_DETAIL_IMAGE_LEN           8

#define L256_DETAIL_ED25519             1

#define L256_DETAIL_MAP                 1
#define L256_DETAIL_HASH_MISMATCH       2
#define L256_DETAIL_UNMAP               3

void l256_verify_plugin(Handle targetProcess);
