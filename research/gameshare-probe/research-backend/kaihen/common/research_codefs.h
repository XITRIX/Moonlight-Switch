// SPDX-License-Identifier: MIT
// Private research extension. It never exports the retained loader root.
#pragma once
#include <stdint.h>

enum { ResearchCodeFs_Info = 0x47530000, ResearchCodeFs_Open = 0x47530001 };
enum { ResearchCodeFs_Magic = 0x4f525347, ResearchCodeFs_Abi = 1 };
typedef struct {
    uint32_t magic, abi, loader_available, reserved;
} ResearchCodeFsInfo;
typedef struct {
    uint8_t attributes, storage, reserved[6];
    uint64_t program_id;
} ResearchCodeFsOpen;

static inline int researchCodeFsValid(const ResearchCodeFsOpen* in) {
    if (in->storage != 2 || in->attributes > 1) return 0;
    for (unsigned i=0;i<6;i++) if (in->reserved[i]) return 0;
    uint64_t space=in->program_id>>56;
    uint64_t low=in->program_id & UINT64_C(0x00ffffffffffffff);
    return (space==1 || space==4) && low>=0x10000;
}
#ifdef __cplusplus
static_assert(sizeof(ResearchCodeFsOpen)==16);
static_assert(sizeof(ResearchCodeFsInfo)==16);
#endif
