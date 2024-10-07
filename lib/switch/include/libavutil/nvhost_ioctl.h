/*
 * include/uapi/linux/nvhost_ioctl.h
 *
 * Tegra graphics host driver
 *
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __UAPI_LINUX_NVHOST_IOCTL_H
#define __UAPI_LINUX_NVHOST_IOCTL_H

#ifndef __SWITCH__
#   include <linux/ioctl.h>
#   include <linux/types.h>
#else
#   include <switch.h>

#   define _IO       _NV_IO
#   define _IOR      _NV_IOR
#   define _IOW      _NV_IOW
#   define _IOWR     _NV_IOWR

#   define _IOC_DIR  _NV_IOC_DIR
#   define _IOC_TYPE _NV_IOC_TYPE
#   define _IOC_NR   _NV_IOC_NR
#   define _IOC_SIZE _NV_IOC_SIZE
#endif

#define __user

#define NVHOST_INVALID_SYNCPOINT 0xFFFFFFFF
#define NVHOST_NO_TIMEOUT (-1)
#define NVHOST_NO_CONTEXT 0x0
#define NVHOST_IOCTL_MAGIC 'H'
#define NVHOST_PRIORITY_LOW 50
#define NVHOST_PRIORITY_MEDIUM 100
#define NVHOST_PRIORITY_HIGH 150

#define NVHOST_TIMEOUT_FLAG_DISABLE_DUMP    0

#define NVHOST_SUBMIT_VERSION_V0            0x0
#define NVHOST_SUBMIT_VERSION_V1            0x1
#define NVHOST_SUBMIT_VERSION_V2            0x2
#define NVHOST_SUBMIT_VERSION_MAX_SUPPORTED NVHOST_SUBMIT_VERSION_V2

struct nvhost_cmdbuf {
    uint32_t mem;
    uint32_t offset;
    uint32_t words;
} __attribute__((packed));

struct nvhost_cmdbuf_ext {
    int32_t  pre_fence;
    uint32_t reserved;
};

struct nvhost_reloc {
    uint32_t cmdbuf_mem;
    uint32_t cmdbuf_offset;
    uint32_t target;
    uint32_t target_offset;
};

struct nvhost_reloc_shift {
    uint32_t shift;
} __attribute__((packed));

#define NVHOST_RELOC_TYPE_DEFAULT    0
#define NVHOST_RELOC_TYPE_PITCH_LINEAR    1
#define NVHOST_RELOC_TYPE_BLOCK_LINEAR    2
#define NVHOST_RELOC_TYPE_NVLINK    3
struct nvhost_reloc_type {
    uint32_t reloc_type;
    uint32_t padding;
};

struct nvhost_waitchk {
    uint32_t mem;
    uint32_t offset;
    uint32_t syncpt_id;
    uint32_t thresh;
};

struct nvhost_syncpt_incr {
    uint32_t syncpt_id;
    uint32_t syncpt_incrs;
};

struct nvhost_get_param_args {
    uint32_t value;
} __attribute__((packed));

struct nvhost_get_param_arg {
    uint32_t param;
    uint32_t value;
};

struct nvhost_get_client_managed_syncpt_arg {
    uint64_t name;
    uint32_t param;
    uint32_t value;
};

struct nvhost_free_client_managed_syncpt_arg {
    uint32_t param;
    uint32_t value;
};

struct nvhost_channel_open_args {
    int32_t  channel_fd;
};

struct nvhost_set_syncpt_name_args {
    uint64_t name;
    uint32_t syncpt_id;
    uint32_t padding;
};

struct nvhost_set_nvmap_fd_args {
    uint32_t fd;
} __attribute__((packed));

enum nvhost_clk_attr {
    NVHOST_CLOCK = 0,
    NVHOST_BW,
    NVHOST_PIXELRATE,
    NVHOST_BW_KHZ,
};

/*
 * moduleid[15:0]  => module id
 * moduleid[24:31] => nvhost_clk_attr
 */
#define NVHOST_MODULE_ID_BIT_POS    0
#define NVHOST_MODULE_ID_BIT_WIDTH  16
#define NVHOST_CLOCK_ATTR_BIT_POS   24
#define NVHOST_CLOCK_ATTR_BIT_WIDTH 8
struct nvhost_clk_rate_args {
    uint32_t rate;
    uint32_t moduleid;
};

struct nvhost_set_timeout_args {
    uint32_t timeout;
} __attribute__((packed));

struct nvhost_set_timeout_ex_args {
    uint32_t timeout;
    uint32_t flags;
};

struct nvhost_set_priority_args {
    uint32_t priority;
} __attribute__((packed));

struct nvhost_set_error_notifier {
    uint64_t offset;
    uint64_t size;
    uint32_t mem;
    uint32_t padding;
};

struct nvhost32_ctrl_module_regrdwr_args {
    uint32_t id;
    uint32_t num_offsets;
    uint32_t block_size;
    uint32_t offsets;
    uint32_t values;
    uint32_t write;
};

struct nvhost_ctrl_module_regrdwr_args {
    uint32_t id;
    uint32_t num_offsets;
    uint32_t block_size;
    uint32_t write;
    uint64_t offsets;
    uint64_t values;
};

struct nvhost32_submit_args {
    uint32_t submit_version;
    uint32_t num_syncpt_incrs;
    uint32_t num_cmdbufs;
    uint32_t num_relocs;
    uint32_t num_waitchks;
    uint32_t timeout;
    uint32_t syncpt_incrs;
    uint32_t cmdbufs;
    uint32_t relocs;
    uint32_t reloc_shifts;
    uint32_t waitchks;
    uint32_t waitbases;
    uint32_t class_ids;

    uint32_t pad[2];        /* future expansion */

    uint32_t fences;
    uint32_t fence;        /* Return value */
} __attribute__((packed));

#define NVHOST_SUBMIT_FLAG_SYNC_FENCE_FD    0
#define NVHOST_SUBMIT_MAX_NUM_SYNCPT_INCRS    10

struct nvhost_submit_args {
    uint32_t submit_version;
    uint32_t num_syncpt_incrs;
    uint32_t num_cmdbufs;
    uint32_t num_relocs;
    uint32_t num_waitchks;
    uint32_t timeout;
    uint32_t flags;
    uint32_t fence;        /* Return value */
    uint64_t syncpt_incrs;
    uint64_t cmdbuf_exts;

    uint32_t checksum_methods;
    uint32_t checksum_falcon_methods;

    uint64_t pad[1];        /* future expansion */

    uint64_t reloc_types;
    uint64_t cmdbufs;
    uint64_t relocs;
    uint64_t reloc_shifts;
    uint64_t waitchks;
    uint64_t waitbases;
    uint64_t class_ids;
    uint64_t fences;
};

struct nvhost_set_ctxswitch_args {
    uint32_t num_cmdbufs_save;
    uint32_t num_save_incrs;
    uint32_t save_incrs;
    uint32_t save_waitbases;
    uint32_t cmdbuf_save;
    uint32_t num_cmdbufs_restore;
    uint32_t num_restore_incrs;
    uint32_t restore_incrs;
    uint32_t restore_waitbases;
    uint32_t cmdbuf_restore;
    uint32_t num_relocs;
    uint32_t relocs;
    uint32_t reloc_shifts;

    uint32_t pad;
};

struct nvhost_channel_buffer {
    uint32_t dmabuf_fd;    /* in */
    uint32_t reserved0;    /* reserved, must be 0 */
    uint64_t reserved1[2];    /* reserved, must be 0 */
    uint64_t address;        /* out, device view to the buffer */
};

struct nvhost_channel_unmap_buffer_args {
    uint32_t num_buffers;    /* in, number of buffers to unmap */
    uint32_t reserved;        /* reserved, must be 0 */
    uint64_t table_address;    /* pointer to beginning of buffer */
};

struct nvhost_channel_map_buffer_args {
    uint32_t num_buffers;    /* in, number of buffers to map */
    uint32_t reserved;        /* reserved, must be 0 */
    uint64_t table_address;    /* pointer to beginning of buffer */
};

#define NVHOST_IOCTL_CHANNEL_GET_SYNCPOINTS    \
    _IOR(NVHOST_IOCTL_MAGIC, 2, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_WAITBASES    \
    _IOR(NVHOST_IOCTL_MAGIC, 3, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_MODMUTEXES    \
    _IOR(NVHOST_IOCTL_MAGIC, 4, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD    \
    _IOW(NVHOST_IOCTL_MAGIC, 5, struct nvhost_set_nvmap_fd_args)
#define NVHOST_IOCTL_CHANNEL_NULL_KICKOFF    \
    _IOR(NVHOST_IOCTL_MAGIC, 6, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_CLK_RATE        \
    _IOWR(NVHOST_IOCTL_MAGIC, 9, struct nvhost_clk_rate_args)
#define NVHOST_IOCTL_CHANNEL_SET_CLK_RATE        \
    _IOW(NVHOST_IOCTL_MAGIC, 10, struct nvhost_clk_rate_args)
#define NVHOST_IOCTL_CHANNEL_SET_TIMEOUT    \
    _IOW(NVHOST_IOCTL_MAGIC, 11, struct nvhost_set_timeout_args)
#define NVHOST_IOCTL_CHANNEL_GET_TIMEDOUT    \
    _IOR(NVHOST_IOCTL_MAGIC, 12, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SET_PRIORITY    \
    _IOW(NVHOST_IOCTL_MAGIC, 13, struct nvhost_set_priority_args)
#define    NVHOST32_IOCTL_CHANNEL_MODULE_REGRDWR    \
    _IOWR(NVHOST_IOCTL_MAGIC, 14, struct nvhost32_ctrl_module_regrdwr_args)
#define NVHOST32_IOCTL_CHANNEL_SUBMIT        \
    _IOWR(NVHOST_IOCTL_MAGIC, 15, struct nvhost32_submit_args)
#define NVHOST_IOCTL_CHANNEL_GET_SYNCPOINT    \
    _IOWR(NVHOST_IOCTL_MAGIC, 16, struct nvhost_get_param_arg)
#define NVHOST_IOCTL_CHANNEL_GET_WAITBASE    \
    _IOWR(NVHOST_IOCTL_MAGIC, 17, struct nvhost_get_param_arg)
#define NVHOST_IOCTL_CHANNEL_SET_TIMEOUT_EX    \
    _IOWR(NVHOST_IOCTL_MAGIC, 18, struct nvhost_set_timeout_ex_args)
#define NVHOST_IOCTL_CHANNEL_GET_CLIENT_MANAGED_SYNCPOINT    \
    _IOWR(NVHOST_IOCTL_MAGIC, 19, struct nvhost_get_client_managed_syncpt_arg)
#define NVHOST_IOCTL_CHANNEL_FREE_CLIENT_MANAGED_SYNCPOINT    \
    _IOWR(NVHOST_IOCTL_MAGIC, 20, struct nvhost_free_client_managed_syncpt_arg)
#define NVHOST_IOCTL_CHANNEL_GET_MODMUTEX    \
    _IOWR(NVHOST_IOCTL_MAGIC, 23, struct nvhost_get_param_arg)
#define NVHOST_IOCTL_CHANNEL_SET_CTXSWITCH    \
    _IOWR(NVHOST_IOCTL_MAGIC, 25, struct nvhost_set_ctxswitch_args)

/* ioctls added for 64bit compatibility */
#define NVHOST_IOCTL_CHANNEL_SUBMIT    \
    _IOWR(NVHOST_IOCTL_MAGIC, 26, struct nvhost_submit_args)
#define    NVHOST_IOCTL_CHANNEL_MODULE_REGRDWR    \
    _IOWR(NVHOST_IOCTL_MAGIC, 27, struct nvhost_ctrl_module_regrdwr_args)

#define    NVHOST_IOCTL_CHANNEL_MAP_BUFFER    \
    _IOWR(NVHOST_IOCTL_MAGIC, 28, struct nvhost_channel_map_buffer_args)
#define    NVHOST_IOCTL_CHANNEL_UNMAP_BUFFER    \
    _IOWR(NVHOST_IOCTL_MAGIC, 29, struct nvhost_channel_unmap_buffer_args)

#define NVHOST_IOCTL_CHANNEL_SET_SYNCPOINT_NAME    \
    _IOW(NVHOST_IOCTL_MAGIC, 30, struct nvhost_set_syncpt_name_args)

#define NVHOST_IOCTL_CHANNEL_SET_ERROR_NOTIFIER  \
    _IOWR(NVHOST_IOCTL_MAGIC, 111, struct nvhost_set_error_notifier)
#define NVHOST_IOCTL_CHANNEL_OPEN    \
    _IOR(NVHOST_IOCTL_MAGIC,  112, struct nvhost_channel_open_args)

#define NVHOST_IOCTL_CHANNEL_LAST    \
    _IOC_NR(NVHOST_IOCTL_CHANNEL_OPEN)
#define NVHOST_IOCTL_CHANNEL_MAX_ARG_SIZE sizeof(struct nvhost_submit_args)

struct nvhost_ctrl_syncpt_read_args {
    uint32_t id;
    uint32_t value;
};

struct nvhost_ctrl_syncpt_incr_args {
    uint32_t id;
} __attribute__((packed));

struct nvhost_ctrl_syncpt_wait_args {
    uint32_t id;
    uint32_t thresh;
    int32_t  timeout;
} __attribute__((packed));

struct nvhost_ctrl_syncpt_waitex_args {
    uint32_t id;
    uint32_t thresh;
    int32_t  timeout;
    uint32_t value;
};

struct nvhost_ctrl_syncpt_waitmex_args {
    uint32_t id;
    uint32_t thresh;
    int32_t  timeout;
    uint32_t value;
    uint32_t tv_sec;
    uint32_t tv_nsec;
    uint32_t clock_id;
    uint32_t reserved;
};

struct nvhost_ctrl_sync_fence_info {
    uint32_t id;
    uint32_t thresh;
};

struct nvhost32_ctrl_sync_fence_create_args {
    uint32_t num_pts;
    uint64_t pts; /* struct nvhost_ctrl_sync_fence_info* */
    uint64_t name; /* const char* */
    int32_t  fence_fd; /* fd of new fence */
};

struct nvhost_ctrl_sync_fence_create_args {
    uint32_t num_pts;
    int32_t  fence_fd; /* fd of new fence */
    uint64_t pts; /* struct nvhost_ctrl_sync_fence_info* */
    uint64_t name; /* const char* */
};

struct nvhost_ctrl_sync_fence_name_args {
    uint64_t name; /* const char* for name */
    int32_t  fence_fd; /* fd of fence */
};

struct nvhost_ctrl_module_mutex_args {
    uint32_t id;
    uint32_t lock;
};

enum nvhost_module_id {
    NVHOST_MODULE_NONE = -1,
    NVHOST_MODULE_DISPLAY_A = 0,
    NVHOST_MODULE_DISPLAY_B,
    NVHOST_MODULE_VI,
    NVHOST_MODULE_ISP,
    NVHOST_MODULE_MPE,
    NVHOST_MODULE_MSENC,
    NVHOST_MODULE_TSEC,
    NVHOST_MODULE_GPU,
    NVHOST_MODULE_VIC,
    NVHOST_MODULE_NVDEC,
    NVHOST_MODULE_NVJPG,
    NVHOST_MODULE_VII2C,
    NVHOST_MODULE_NVENC1,
    NVHOST_MODULE_NVDEC1,
    NVHOST_MODULE_NVCSI,
    NVHOST_MODULE_TSECB = (1<<16) | NVHOST_MODULE_TSEC,
};

struct nvhost_characteristics {
#define NVHOST_CHARACTERISTICS_GFILTER (1 << 0)
#define NVHOST_CHARACTERISTICS_RESOURCE_PER_CHANNEL_INSTANCE (1 << 1)
#define NVHOST_CHARACTERISTICS_SUPPORT_PREFENCES (1 << 2)
    uint64_t flags;

    uint32_t num_mlocks;
    uint32_t num_syncpts;

    uint32_t syncpts_base;
    uint32_t syncpts_limit;

    uint32_t num_hw_pts;
    uint32_t padding;
};

struct nvhost_ctrl_get_characteristics {
    uint64_t nvhost_characteristics_buf_size;
    uint64_t nvhost_characteristics_buf_addr;
};

struct nvhost_ctrl_check_module_support_args {
    uint32_t module_id;
    uint32_t value;
};

struct nvhost_ctrl_poll_fd_create_args {
    int32_t  fd;
    uint32_t padding;
};

struct nvhost_ctrl_poll_fd_trigger_event_args {
    int32_t  fd;
    uint32_t id;
    uint32_t thresh;
    uint32_t padding;
};

#define NVHOST_IOCTL_CTRL_SYNCPT_READ        \
    _IOWR(NVHOST_IOCTL_MAGIC, 1, struct nvhost_ctrl_syncpt_read_args)
#define NVHOST_IOCTL_CTRL_SYNCPT_INCR        \
    _IOW(NVHOST_IOCTL_MAGIC, 2, struct nvhost_ctrl_syncpt_incr_args)
#define NVHOST_IOCTL_CTRL_SYNCPT_WAIT        \
    _IOW(NVHOST_IOCTL_MAGIC, 3, struct nvhost_ctrl_syncpt_wait_args)

#define NVHOST_IOCTL_CTRL_MODULE_MUTEX        \
    _IOWR(NVHOST_IOCTL_MAGIC, 4, struct nvhost_ctrl_module_mutex_args)
#define NVHOST32_IOCTL_CTRL_MODULE_REGRDWR    \
    _IOWR(NVHOST_IOCTL_MAGIC, 5, struct nvhost32_ctrl_module_regrdwr_args)

#define NVHOST_IOCTL_CTRL_SYNCPT_WAITEX        \
    _IOWR(NVHOST_IOCTL_MAGIC, 6, struct nvhost_ctrl_syncpt_waitex_args)

#define NVHOST_IOCTL_CTRL_GET_VERSION    \
    _IOR(NVHOST_IOCTL_MAGIC, 7, struct nvhost_get_param_args)

#define NVHOST_IOCTL_CTRL_SYNCPT_READ_MAX    \
    _IOWR(NVHOST_IOCTL_MAGIC, 8, struct nvhost_ctrl_syncpt_read_args)

#define NVHOST_IOCTL_CTRL_SYNCPT_WAITMEX    \
    _IOWR(NVHOST_IOCTL_MAGIC, 9, struct nvhost_ctrl_syncpt_waitmex_args)

#define NVHOST32_IOCTL_CTRL_SYNC_FENCE_CREATE    \
    _IOWR(NVHOST_IOCTL_MAGIC, 10, struct nvhost32_ctrl_sync_fence_create_args)
#define NVHOST_IOCTL_CTRL_SYNC_FENCE_CREATE    \
    _IOWR(NVHOST_IOCTL_MAGIC, 11, struct nvhost_ctrl_sync_fence_create_args)
#define NVHOST_IOCTL_CTRL_MODULE_REGRDWR    \
    _IOWR(NVHOST_IOCTL_MAGIC, 12, struct nvhost_ctrl_module_regrdwr_args)
#define NVHOST_IOCTL_CTRL_SYNC_FENCE_SET_NAME  \
    _IOWR(NVHOST_IOCTL_MAGIC, 13, struct nvhost_ctrl_sync_fence_name_args)
#define NVHOST_IOCTL_CTRL_GET_CHARACTERISTICS  \
    _IOWR(NVHOST_IOCTL_MAGIC, 14, struct nvhost_ctrl_get_characteristics)
#define NVHOST_IOCTL_CTRL_CHECK_MODULE_SUPPORT  \
    _IOWR(NVHOST_IOCTL_MAGIC, 15, struct nvhost_ctrl_check_module_support_args)
#define NVHOST_IOCTL_CTRL_POLL_FD_CREATE    \
    _IOR(NVHOST_IOCTL_MAGIC, 16, struct nvhost_ctrl_poll_fd_create_args)
#define NVHOST_IOCTL_CTRL_POLL_FD_TRIGGER_EVENT        \
    _IOW(NVHOST_IOCTL_MAGIC, 17, struct nvhost_ctrl_poll_fd_trigger_event_args)

#define NVHOST_IOCTL_CTRL_LAST            \
    _IOC_NR(NVHOST_IOCTL_CTRL_POLL_FD_TRIGGER_EVENT)
#define NVHOST_IOCTL_CTRL_MAX_ARG_SIZE    \
    sizeof(struct nvhost_ctrl_syncpt_waitmex_args)

#endif
