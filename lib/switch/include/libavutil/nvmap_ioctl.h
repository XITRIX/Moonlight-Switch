/*
 * include/uapi/linux/nvmap.h
 *
 * structure declarations for nvmem and nvmap user-space ioctls
 *
 * Copyright (c) 2009-2020, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __SWITCH__
#include <linux/ioctl.h>
#include <linux/types.h>
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

#ifndef __UAPI_LINUX_NVMAP_H
#define __UAPI_LINUX_NVMAP_H

/*
 * From linux-headers nvidia/include/linux/nvmap.h
 */
#define NVMAP_HEAP_IOVMM   (1ul<<30)

/* common carveout heaps */
#define NVMAP_HEAP_CARVEOUT_IRAM    (1ul<<29)
#define NVMAP_HEAP_CARVEOUT_VPR     (1ul<<28)
#define NVMAP_HEAP_CARVEOUT_TSEC    (1ul<<27)
#define NVMAP_HEAP_CARVEOUT_VIDMEM  (1ul<<26)
#define NVMAP_HEAP_CARVEOUT_IVM     (1ul<<1)
#define NVMAP_HEAP_CARVEOUT_GENERIC (1ul<<0)

#define NVMAP_HEAP_CARVEOUT_MASK    (NVMAP_HEAP_IOVMM - 1)

/* allocation flags */
#define NVMAP_HANDLE_UNCACHEABLE     (0x0ul << 0)
#define NVMAP_HANDLE_WRITE_COMBINE   (0x1ul << 0)
#define NVMAP_HANDLE_INNER_CACHEABLE (0x2ul << 0)
#define NVMAP_HANDLE_CACHEABLE       (0x3ul << 0)
#define NVMAP_HANDLE_CACHE_FLAG      (0x3ul << 0)

#define NVMAP_HANDLE_SECURE          (0x1ul << 2)
#define NVMAP_HANDLE_KIND_SPECIFIED  (0x1ul << 3)
#define NVMAP_HANDLE_COMPR_SPECIFIED (0x1ul << 4)
#define NVMAP_HANDLE_ZEROED_PAGES    (0x1ul << 5)
#define NVMAP_HANDLE_PHYS_CONTIG     (0x1ul << 6)
#define NVMAP_HANDLE_CACHE_SYNC      (0x1ul << 7)
#define NVMAP_HANDLE_CACHE_SYNC_AT_RESERVE      (0x1ul << 8)
#define NVMAP_HANDLE_RO	             (0x1ul << 9)

/*
 * DOC: NvMap Userspace API
 *
 * create a client by opening /dev/nvmap
 * most operations handled via following ioctls
 *
 */
enum {
    NVMAP_HANDLE_PARAM_SIZE = 1,
    NVMAP_HANDLE_PARAM_ALIGNMENT,
    NVMAP_HANDLE_PARAM_BASE,
    NVMAP_HANDLE_PARAM_HEAP,
    NVMAP_HANDLE_PARAM_KIND,
    NVMAP_HANDLE_PARAM_COMPR, /* ignored, to be removed */
};

enum {
    NVMAP_CACHE_OP_WB = 0,
    NVMAP_CACHE_OP_INV,
    NVMAP_CACHE_OP_WB_INV,
};

enum {
    NVMAP_PAGES_UNRESERVE = 0,
    NVMAP_PAGES_RESERVE,
    NVMAP_INSERT_PAGES_ON_UNRESERVE,
    NVMAP_PAGES_PROT_AND_CLEAN,
};

#define NVMAP_ELEM_SIZE_U64 (1 << 31)

struct nvmap_create_handle {
    union {
        struct {
            union {
                /* size will be overwritten */
                uint32_t size; /* CreateHandle */
                int32_t  fd;   /* DmaBufFd or FromFd */
            };
            uint32_t handle;   /* returns nvmap handle */
        };
        struct {
            /* one is input parameter, and other is output parameter
             * since its a union please note that input parameter
             * will be overwritten once ioctl returns
             */
            union {
                uint64_t ivm_id;     /* CreateHandle from ivm*/
                int32_t  ivm_handle; /* Get ivm_id from handle */
            };
        };
        struct {
            union {
                /* size64 will be overwritten */
                uint64_t size64;   /* CreateHandle */
                uint32_t handle64; /* returns nvmap handle */
            };
        };
    };
};

struct nvmap_create_handle_from_va {
    uint64_t va;                /* FromVA*/
    uint32_t size;              /* non-zero for partial memory VMA. zero for end of VMA */
    uint32_t flags;             /* wb/wc/uc/iwb, tag etc. */
    union {
        uint32_t handle;        /* returns nvmap handle */
        uint64_t size64;        /* used when size is 0 */
    };
};

struct nvmap_gup_test {
    uint64_t va;       /* FromVA*/
    uint32_t handle;   /* returns nvmap handle */
    uint32_t result;   /* result=1 for pass, result=-err for failure */
};

struct nvmap_alloc_handle {
    uint32_t handle;       /* nvmap handle */
    uint32_t heap_mask;    /* heaps to allocate from */
    uint32_t flags;        /* wb/wc/uc/iwb etc. */
    uint32_t align;        /* min alignment necessary */
};

struct nvmap_alloc_ivm_handle {
    uint32_t handle;       /* nvmap handle */
    uint32_t heap_mask;    /* heaps to allocate from */
    uint32_t flags;        /* wb/wc/uc/iwb etc. */
    uint32_t align;        /* min alignment necessary */
    uint32_t peer;         /* peer with whom handle must be shared. Used
                           *  only for NVMAP_HEAP_CARVEOUT_IVM
                           */
};

struct nvmap_alloc_kind_handle {
    uint32_t handle;       /* nvmap handle */
    uint32_t heap_mask;
    uint32_t flags;
    uint32_t align;
    uint8_t  kind;
    uint8_t  comp_tags;
};

struct nvmap_map_caller {
    uint32_t handle;       /* nvmap handle */
    uint32_t offset;       /* offset into hmem; should be page-aligned */
    uint32_t length;       /* number of bytes to map */
    uint32_t flags;        /* maps as wb/iwb etc. */
    unsigned long addr;    /* user pointer */
};

#ifdef CONFIG_COMPAT
struct nvmap_map_caller_32 {
    uint32_t handle;       /* nvmap handle */
    uint32_t offset;       /* offset into hmem; should be page-aligned */
    uint32_t length;       /* number of bytes to map */
    uint32_t flags;        /* maps as wb/iwb etc. */
    uint32_t addr;         /* user pointer*/
};
#endif

struct nvmap_rw_handle {
    unsigned long addr;    /* user pointer*/
    uint32_t handle;       /* nvmap handle */
    uint32_t offset;       /* offset into hmem */
    uint32_t elem_size;    /* individual atom size */
    uint32_t hmem_stride;  /* delta in bytes between atoms in hmem */
    uint32_t user_stride;  /* delta in bytes between atoms in user */
    uint32_t count;        /* number of atoms to copy */
};

struct nvmap_rw_handle_64 {
    unsigned long addr;    /* user pointer*/
    uint32_t handle;       /* nvmap handle */
    uint64_t offset;       /* offset into hmem */
    uint64_t elem_size;    /* individual atom size */
    uint64_t hmem_stride;  /* delta in bytes between atoms in hmem */
    uint64_t user_stride;  /* delta in bytes between atoms in user */
    uint64_t count;        /* number of atoms to copy */
};

#ifdef CONFIG_COMPAT
struct nvmap_rw_handle_32 {
    uint32_t addr;         /* user pointer */
    uint32_t handle;       /* nvmap handle */
    uint32_t offset;       /* offset into hmem */
    uint32_t elem_size;    /* individual atom size */
    uint32_t hmem_stride;  /* delta in bytes between atoms in hmem */
    uint32_t user_stride;  /* delta in bytes between atoms in user */
    uint32_t count;        /* number of atoms to copy */
};
#endif

struct nvmap_pin_handle {
    uint32_t *handles;         /* array of handles to pin/unpin */
    unsigned long *addr;       /* array of addresses to return */
    uint32_t count;            /* number of entries in handles */
};

#ifdef CONFIG_COMPAT
struct nvmap_pin_handle_32 {
    uint32_t handles;          /* array of handles to pin/unpin */
    uint32_t addr;             /*  array of addresses to return */
    uint32_t count;            /* number of entries in handles */
};
#endif

struct nvmap_handle_param {
    uint32_t handle;           /* nvmap handle */
    uint32_t param;            /* size/align/base/heap etc. */
    unsigned long result;      /* returns requested info*/
};

#ifdef CONFIG_COMPAT
struct nvmap_handle_param_32 {
    uint32_t handle;           /* nvmap handle */
    uint32_t param;            /* size/align/base/heap etc. */
    uint32_t result;           /* returns requested info*/
};
#endif

struct nvmap_cache_op {
    unsigned long addr;    /* user pointer*/
    uint32_t handle;       /* nvmap handle */
    uint32_t len;          /* bytes to flush */
    int32_t  op;           /* wb/wb_inv/inv */
};

struct nvmap_cache_op_64 {
    unsigned long addr;    /* user pointer*/
    uint32_t handle;       /* nvmap handle */
    uint64_t len;          /* bytes to flush */
    int32_t  op;           /* wb/wb_inv/inv */
};

#ifdef CONFIG_COMPAT
struct nvmap_cache_op_32 {
    uint32_t addr;         /* user pointer*/
    uint32_t handle;       /* nvmap handle */
    uint32_t len;          /* bytes to flush */
    int32_t  op;           /* wb/wb_inv/inv */
};
#endif

struct nvmap_cache_op_list {
    uint64_t handles;      /* Ptr to u32 type array, holding handles */
    uint64_t offsets;      /* Ptr to u32 type array, holding offsets
                           * into handle mem */
    uint64_t sizes;        /* Ptr to u32 type array, holindg sizes of memory
                           * regions within each handle */
    uint32_t nr;           /* Number of handles */
    int32_t  op;           /* wb/wb_inv/inv */
};

struct nvmap_debugfs_handles_header {
    uint8_t version;
};

struct nvmap_debugfs_handles_entry {
    uint64_t base;
    uint64_t size;
    uint32_t flags;
    uint32_t share_count;
    uint64_t mapped_size;
};

struct nvmap_set_tag_label {
    uint32_t tag;
    uint32_t len;          /* in: label length
                           out: number of characters copied */
    uint64_t addr;         /* in: pointer to label or NULL to remove */
};

struct nvmap_available_heaps {
    uint64_t heaps;        /* heaps bitmask */
};

struct nvmap_heap_size {
    uint32_t heap;
    uint64_t size;
};

/**
 * Struct used while querying heap parameters
 */
struct nvmap_query_heap_params {
    uint32_t heap_mask;
    uint32_t flags;
    uint8_t contig;
    uint64_t total;
    uint64_t free;
    uint64_t largest_free_block;
};

struct nvmap_handle_parameters {
    uint8_t contig;
    uint32_t import_id;
    uint32_t handle;
    uint32_t heap_number;
    uint32_t access_flags;
    uint64_t heap;
    uint64_t align;
    uint64_t coherency;
    uint64_t size;
};

#define NVMAP_IOC_MAGIC 'N'

/* Creates a new memory handle. On input, the argument is the size of the new
 * handle; on return, the argument is the name of the new handle
 */
#define NVMAP_IOC_CREATE  _IOWR(NVMAP_IOC_MAGIC, 0, struct nvmap_create_handle)
#define NVMAP_IOC_CREATE_64 \
    _IOWR(NVMAP_IOC_MAGIC, 1, struct nvmap_create_handle)
#define NVMAP_IOC_FROM_ID _IOWR(NVMAP_IOC_MAGIC, 2, struct nvmap_create_handle)

/* Actually allocates memory for the specified handle */
#define NVMAP_IOC_ALLOC    _IOW(NVMAP_IOC_MAGIC, 3, struct nvmap_alloc_handle)

/* Frees a memory handle, unpinning any pinned pages and unmapping any mappings
 */
#define NVMAP_IOC_FREE       _IO(NVMAP_IOC_MAGIC, 4)

/* Maps the region of the specified handle into a user-provided virtual address
 * that was previously created via an mmap syscall on this fd */
#define NVMAP_IOC_MMAP       _IOWR(NVMAP_IOC_MAGIC, 5, struct nvmap_map_caller)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_MMAP_32    _IOWR(NVMAP_IOC_MAGIC, 5, struct nvmap_map_caller_32)
#endif

/* Reads/writes data (possibly strided) from a user-provided buffer into the
 * hmem at the specified offset */
#define NVMAP_IOC_WRITE      _IOW(NVMAP_IOC_MAGIC, 6, struct nvmap_rw_handle)
#define NVMAP_IOC_READ       _IOW(NVMAP_IOC_MAGIC, 7, struct nvmap_rw_handle)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_WRITE_32   _IOW(NVMAP_IOC_MAGIC, 6, struct nvmap_rw_handle_32)
#define NVMAP_IOC_READ_32    _IOW(NVMAP_IOC_MAGIC, 7, struct nvmap_rw_handle_32)
#endif
#define NVMAP_IOC_WRITE_64 \
    _IOW(NVMAP_IOC_MAGIC, 6, struct nvmap_rw_handle_64)
#define NVMAP_IOC_READ_64 \
    _IOW(NVMAP_IOC_MAGIC, 7, struct nvmap_rw_handle_64)

#define NVMAP_IOC_PARAM _IOWR(NVMAP_IOC_MAGIC, 8, struct nvmap_handle_param)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_PARAM_32 _IOWR(NVMAP_IOC_MAGIC, 8, struct nvmap_handle_param_32)
#endif

/* Pins a list of memory handles into IO-addressable memory (either IOVMM
 * space or physical memory, depending on the allocation), and returns the
 * address. Handles may be pinned recursively. */
#define NVMAP_IOC_PIN_MULT      _IOWR(NVMAP_IOC_MAGIC, 10, struct nvmap_pin_handle)
#define NVMAP_IOC_UNPIN_MULT    _IOW(NVMAP_IOC_MAGIC, 11, struct nvmap_pin_handle)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_PIN_MULT_32   _IOWR(NVMAP_IOC_MAGIC, 10, struct nvmap_pin_handle_32)
#define NVMAP_IOC_UNPIN_MULT_32 _IOW(NVMAP_IOC_MAGIC, 11, struct nvmap_pin_handle_32)
#endif

#define NVMAP_IOC_CACHE      _IOW(NVMAP_IOC_MAGIC, 12, struct nvmap_cache_op)
#define NVMAP_IOC_CACHE_64   _IOW(NVMAP_IOC_MAGIC, 12, struct nvmap_cache_op_64)
#ifdef CONFIG_COMPAT
#define NVMAP_IOC_CACHE_32  _IOW(NVMAP_IOC_MAGIC, 12, struct nvmap_cache_op_32)
#endif

/* Returns a global ID usable to allow a remote process to create a handle
 * reference to the same handle */
#define NVMAP_IOC_GET_ID  _IOWR(NVMAP_IOC_MAGIC, 13, struct nvmap_create_handle)

/* Returns a dma-buf fd usable to allow a remote process to create a handle
 * reference to the same handle */
#define NVMAP_IOC_SHARE  _IOWR(NVMAP_IOC_MAGIC, 14, struct nvmap_create_handle)

/* Returns a file id that allows a remote process to create a handle
 * reference to the same handle */
#define NVMAP_IOC_GET_FD  _IOWR(NVMAP_IOC_MAGIC, 15, struct nvmap_create_handle)

/* Create a new memory handle from file id passed */
#define NVMAP_IOC_FROM_FD _IOWR(NVMAP_IOC_MAGIC, 16, struct nvmap_create_handle)

/* Perform cache maintenance on a list of handles. */
#define NVMAP_IOC_CACHE_LIST _IOW(NVMAP_IOC_MAGIC, 17,    \
                  struct nvmap_cache_op_list)
/* Perform reserve operation on a list of handles. */
#define NVMAP_IOC_RESERVE _IOW(NVMAP_IOC_MAGIC, 18,    \
                  struct nvmap_cache_op_list)

#define NVMAP_IOC_FROM_IVC_ID _IOWR(NVMAP_IOC_MAGIC, 19, struct nvmap_create_handle)
#define NVMAP_IOC_GET_IVC_ID _IOWR(NVMAP_IOC_MAGIC, 20, struct nvmap_create_handle)
#define NVMAP_IOC_GET_IVM_HEAPS _IOR(NVMAP_IOC_MAGIC, 21, unsigned int)

/* Create a new memory handle from VA passed */
#define NVMAP_IOC_FROM_VA _IOWR(NVMAP_IOC_MAGIC, 22, struct nvmap_create_handle_from_va)

#define NVMAP_IOC_GUP_TEST _IOWR(NVMAP_IOC_MAGIC, 23, struct nvmap_gup_test)

/* Define a label for allocation tag */
#define NVMAP_IOC_SET_TAG_LABEL    _IOW(NVMAP_IOC_MAGIC, 24, struct nvmap_set_tag_label)

#define NVMAP_IOC_GET_AVAILABLE_HEAPS \
    _IOR(NVMAP_IOC_MAGIC, 25, struct nvmap_available_heaps)

#define NVMAP_IOC_GET_HEAP_SIZE \
    _IOR(NVMAP_IOC_MAGIC, 26, struct nvmap_heap_size)

#define NVMAP_IOC_PARAMETERS \
    _IOR(NVMAP_IOC_MAGIC, 27, struct nvmap_handle_parameters)
/* START of T124 IOCTLS */
/* Actually allocates memory for the specified handle, with kind */
#define NVMAP_IOC_ALLOC_KIND _IOW(NVMAP_IOC_MAGIC, 100, struct nvmap_alloc_kind_handle)

/* Actually allocates memory from IVM heaps */
#define NVMAP_IOC_ALLOC_IVM _IOW(NVMAP_IOC_MAGIC, 101, struct nvmap_alloc_ivm_handle)

/* Allocate seperate memory for VPR */
#define NVMAP_IOC_VPR_FLOOR_SIZE _IOW(NVMAP_IOC_MAGIC, 102, uint32_t)

/* Get heap parameters such as total and frre size */
#define NVMAP_IOC_QUERY_HEAP_PARAMS _IOR(NVMAP_IOC_MAGIC, 105, \
        struct nvmap_query_heap_params)

#define NVMAP_IOC_MAXNR (_IOC_NR(NVMAP_IOC_QUERY_HEAP_PARAMS))

#endif /* __UAPI_LINUX_NVMAP_H */
