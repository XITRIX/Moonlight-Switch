/*
 * Copyright (c) 2024 averne <averne381@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with FFmpeg; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef AVUTIL_NVTEGRA_H
#define AVUTIL_NVTEGRA_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"

#include "nvhost_ioctl.h"
#include "nvmap_ioctl.h"

typedef struct AVNVTegraChannel {
#ifndef __SWITCH__
    int fd;
    int module_id;
#else
    NvChannel channel;
#endif

    uint32_t syncpt;

#ifdef __SWITCH__
    MmuRequest mmu_request;
#endif
    uint32_t clock;
} AVNVTegraChannel;

typedef struct AVNVTegraMap {
#ifndef __SWITCH__
    uint32_t handle;
    uint32_t size;
    void *cpu_addr;
#else
    NvMap map;
    uint32_t iova;
    uint32_t owner;
#endif
    bool is_linear;
} AVNVTegraMap;

typedef struct AVNVTegraCmdbuf {
    AVNVTegraMap *map;

    uint32_t mem_offset, mem_size;

    uint32_t *cur_word;

    struct nvhost_cmdbuf       *cmdbufs;
#ifndef __SWITCH__
    struct nvhost_cmdbuf_ext   *cmdbuf_exts;
    uint32_t                   *class_ids;
#endif
    uint32_t num_cmdbufs;

#ifndef __SWITCH__
    struct nvhost_reloc        *relocs;
    struct nvhost_reloc_type   *reloc_types;
    struct nvhost_reloc_shift  *reloc_shifts;
    uint32_t num_relocs;
#endif

    struct nvhost_syncpt_incr  *syncpt_incrs;
#ifndef __SWITCH__
    uint32_t                   *fences;
#endif
    uint32_t num_syncpt_incrs;

#ifndef __SWITCH__
    struct nvhost_waitchk      *waitchks;
    uint32_t num_waitchks;
#endif
} AVNVTegraCmdbuf;

typedef struct AVNVTegraJobPool {
    /*
     * Pool object for job allocation
     */
    AVBufferPool *pool;

    /*
     * Hardware channel the jobs will be submitted to
     */
    AVNVTegraChannel *channel;

    /*
     * Total size of the input memory-mapped buffer
     */
    size_t input_map_size;

    /*
     * Offset of the command data within the input map
     */
    off_t cmdbuf_off;

    /*
     * Maximum memory usable by the command buffer
     */
    size_t max_cmdbuf_size;
} AVNVTegraJobPool;

typedef struct AVNVTegraJob {
    /*
     * Memory-mapped buffer for command buffers, metadata structures, ...
     */
    AVNVTegraMap input_map;

    /*
     * Object for command recording
     */
    AVNVTegraCmdbuf cmdbuf;

    /*
     * Fence indicating completion of the job
     */
    uint32_t fence;
} AVNVTegraJob;

AVBufferRef *av_nvtegra_driver_init(void);

int av_nvtegra_channel_open(AVNVTegraChannel *channel, const char *dev);
int av_nvtegra_channel_close(AVNVTegraChannel *channel);
int av_nvtegra_channel_get_clock_rate(AVNVTegraChannel *channel, uint32_t moduleid, uint32_t *clock_rate);
int av_nvtegra_channel_set_clock_rate(AVNVTegraChannel *channel, uint32_t moduleid, uint32_t clock_rate);
int av_nvtegra_channel_submit(AVNVTegraChannel *channel, AVNVTegraCmdbuf *cmdbuf, uint32_t *fence);
int av_nvtegra_channel_set_submit_timeout(AVNVTegraChannel *channel, uint32_t timeout_ms);

int av_nvtegra_syncpt_wait(AVNVTegraChannel *channel, uint32_t threshold, int32_t timeout);

int av_nvtegra_map_allocate(AVNVTegraMap *map, AVNVTegraChannel *owner, uint32_t size,
                            uint32_t align, int heap_mask, int flags);
int av_nvtegra_map_free(AVNVTegraMap *map);
int av_nvtegra_map_from_va(AVNVTegraMap *map, AVNVTegraChannel *owner, void *mem,
                           uint32_t size, uint32_t align, uint32_t flags);
int av_nvtegra_map_close(AVNVTegraMap *map);
int av_nvtegra_map_map(AVNVTegraMap *map);
int av_nvtegra_map_unmap(AVNVTegraMap *map);
int av_nvtegra_map_cache_op(AVNVTegraMap *map, int op, void *addr, size_t len);
int av_nvtegra_map_realloc(AVNVTegraMap *map, uint32_t size, uint32_t align, int heap_mask, int flags);

static inline int av_nvtegra_map_create(AVNVTegraMap *map, AVNVTegraChannel *owner, uint32_t size, uint32_t align,
                                        int heap_mask, int flags)
{
    int err;

    err = av_nvtegra_map_allocate(map, owner, size, align, heap_mask, flags);
    if (err < 0)
        return err;

    return av_nvtegra_map_map(map);
}

static inline int av_nvtegra_map_destroy(AVNVTegraMap *map) {
    int err;

    err = av_nvtegra_map_unmap(map);
    if (err < 0)
        return err;

    return av_nvtegra_map_free(map);
}

int av_nvtegra_cmdbuf_init(AVNVTegraCmdbuf *cmdbuf);
int av_nvtegra_cmdbuf_deinit(AVNVTegraCmdbuf *cmdbuf);
int av_nvtegra_cmdbuf_add_memory(AVNVTegraCmdbuf *cmdbuf, AVNVTegraMap *map, uint32_t offset, uint32_t size);
int av_nvtegra_cmdbuf_clear(AVNVTegraCmdbuf *cmdbuf);
int av_nvtegra_cmdbuf_begin(AVNVTegraCmdbuf *cmdbuf, uint32_t class_id);
int av_nvtegra_cmdbuf_end(AVNVTegraCmdbuf *cmdbuf);
int av_nvtegra_cmdbuf_push_word(AVNVTegraCmdbuf *cmdbuf, uint32_t word);
int av_nvtegra_cmdbuf_push_value(AVNVTegraCmdbuf *cmdbuf, uint32_t offset, uint32_t word);
int av_nvtegra_cmdbuf_push_reloc(AVNVTegraCmdbuf *cmdbuf, uint32_t offset, AVNVTegraMap *target, uint32_t target_offset,
                                 int reloc_type, int shift);
int av_nvtegra_cmdbuf_push_syncpt_incr(AVNVTegraCmdbuf *cmdbuf, uint32_t syncpt);
int av_nvtegra_cmdbuf_push_wait(AVNVTegraCmdbuf *cmdbuf, uint32_t syncpt, uint32_t fence);
int av_nvtegra_cmdbuf_add_syncpt_incr(AVNVTegraCmdbuf *cmdbuf, uint32_t syncpt, uint32_t fence);
int av_nvtegra_cmdbuf_add_waitchk(AVNVTegraCmdbuf *cmdbuf, uint32_t syncpt, uint32_t fence);

/*
 * Job allocation and submission routines
 */
int av_nvtegra_job_pool_init(AVNVTegraJobPool *pool, AVNVTegraChannel *channel,
                             size_t input_map_size, off_t cmdbuf_off, size_t max_cmdbuf_size);
int av_nvtegra_job_pool_uninit(AVNVTegraJobPool *pool);
AVBufferRef *av_nvtegra_job_pool_get(AVNVTegraJobPool *pool);

int av_nvtegra_job_submit(AVNVTegraJobPool *pool, AVNVTegraJob *job);
int av_nvtegra_job_wait(AVNVTegraJobPool *pool, AVNVTegraJob *job, int timeout);

static inline uint32_t av_nvtegra_map_get_handle(AVNVTegraMap *map) {
#ifndef __SWITCH__
    return map->handle;
#else
    return map->map.handle;
#endif
}

static inline void *av_nvtegra_map_get_addr(AVNVTegraMap *map) {
#ifndef __SWITCH__
    return map->cpu_addr;
#else
    return map->map.cpu_addr;
#endif
}

static inline uint32_t av_nvtegra_map_get_size(AVNVTegraMap *map) {
#ifndef __SWITCH__
    return map->size;
#else
    return map->map.size;
#endif
}

/* Addresses are shifted by 8 bits in the command buffer, requiring an alignment to 256 */
#define AV_NVTEGRA_MAP_ALIGN (1 << 8)

#define AV_NVTEGRA_VALUE(offset, field, value)                                                    \
    ((value &                                                                                     \
    ((uint32_t)((UINT64_C(1) << ((1?offset ## _ ## field) - (0?offset ## _ ## field) + 1)) - 1))) \
    << (0?offset ## _ ## field))

#define AV_NVTEGRA_ENUM(offset, field, value)                                                     \
    ((offset ## _ ## field ## _ ## value &                                                        \
    ((uint32_t)((UINT64_C(1) << ((1?offset ## _ ## field) - (0?offset ## _ ## field) + 1)) - 1))) \
    << (0?offset ## _ ## field))

#define AV_NVTEGRA_PUSH_VALUE(cmdbuf, offset, value) ({                                  \
    int _err = av_nvtegra_cmdbuf_push_value(cmdbuf, (offset) / sizeof(uint32_t), value); \
    if (_err < 0)                                                                        \
        return _err;                                                                     \
})

#define AV_NVTEGRA_PUSH_RELOC(cmdbuf, offset, target, target_offset, type) ({    \
    int _err = av_nvtegra_cmdbuf_push_reloc(cmdbuf, (offset) / sizeof(uint32_t), \
                                            target, target_offset, type, 8);     \
    if (_err < 0)                                                                \
        return _err;                                                             \
})

#endif /* AVUTIL_NVTEGRA_H */
