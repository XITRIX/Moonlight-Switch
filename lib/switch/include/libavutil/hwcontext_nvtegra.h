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

#ifndef AVUTIL_HWCONTEXT_NVTEGRA_H
#define AVUTIL_HWCONTEXT_NVTEGRA_H

#include <stdint.h>

#include "hwcontext.h"
#include "buffer.h"
#include "frame.h"
#include "pixfmt.h"

#include "nvtegra.h"

/*
 * Encode a hardware revision into a version number
 */
#define AV_NVTEGRA_ENCODE_REV(maj, min) (((maj & 0xff) << 8) | (min & 0xff))

/*
 * Decode a version number
 */
static inline void av_nvtegra_decode_rev(int rev, int *maj, int *min) {
    *maj = (rev >> 8) & 0xff;
    *min = (rev >> 0) & 0xff;
}

/**
 * @file
 * API-specific header for AV_HWDEVICE_TYPE_NVTEGRA.
 *
 * For user-allocated pools, AVHWFramesContext.pool must return AVBufferRefs
 * with the data pointer set to an AVNVTegraMap.
 */

typedef struct AVNVTegraDeviceContext {
    /*
     * Hardware multimedia engines
     */
    AVNVTegraChannel nvdec_channel, nvenc_channel, nvjpg_channel, vic_channel;

    /*
     * Hardware revisions for associated engines, or 0 if invalid
     */
    int nvdec_version, nvenc_version, nvjpg_version, vic_version;
} AVNVTegraDeviceContext;

typedef struct AVNVTegraFrame {
    /*
     * Reference to an AVNVTegraMap object
     */
    AVBufferRef *map_ref;
} AVNVTegraFrame;

/*
 * Helper to retrieve a map object from the corresponding frame
 */
static inline AVNVTegraMap *av_nvtegra_frame_get_fbuf_map(const AVFrame *frame) {
    return (AVNVTegraMap *)((AVNVTegraFrame *)frame->buf[0]->data)->map_ref->data;
}

/*
 * Converts a pixel format to the equivalent code for the VIC engine
 */
int av_nvtegra_pixfmt_to_vic(enum AVPixelFormat fmt);

/*
 * Dynamic frequency scaling routines
 */
int av_nvtegra_dfs_init(AVHWDeviceContext *ctx, AVNVTegraChannel *channel, int width, int height, double framerate_hz);
int av_nvtegra_dfs_update(AVHWDeviceContext *ctx, AVNVTegraChannel *channel, int bitstream_len, int decode_cycles);
int av_nvtegra_dfs_uninit(AVHWDeviceContext *ctx, AVNVTegraChannel *channel);

#endif /* AVUTIL_HWCONTEXT_NVTEGRA_H */
