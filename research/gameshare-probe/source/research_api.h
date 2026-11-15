// SPDX-License-Identifier: MIT
// Versioned ABI shared by the agent and freestanding AArch64 research snippets.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define RESEARCH_ABI 1
#define RESEARCH_STATE_SIZE 65536
#define RESEARCH_OUTPUT_SIZE 65536
#define RESEARCH_CODE_LIMIT (256*1024)
#define RESEARCH_INPUT_LIMIT 65536

typedef struct {
    uint32_t attributes; // SDK SfBufferAttr_* bitmask; use the matching SDK header.
    uint32_t size;
    void *data;
} ResearchBuffer;
typedef struct {
    uint32_t command, input_size, output_size, send_pid;
    const void *input;
    void *output;
    uint32_t buffer_count, input_handle_count, input_object_count, output_object_count;
    ResearchBuffer buffers[4];
    uint32_t input_handles[4], input_objects[4];
    uint32_t output_handle_attributes[4]; // 0 none, 1 copy, 2 move.
} ResearchIpc;
typedef struct {
    uint32_t result;
    uint32_t objects[4]; // Agent resource IDs, retained between jobs.
    uint32_t handles[4]; // Kernel handles, retained until explicitly closed/process exit.
} ResearchReply;
typedef struct {
    uint64_t address, size;
    uint32_t type, attributes, permissions, device_refcount, ipc_refcount;
    uint32_t reserved;
} ResearchMemoryInfo;
typedef struct {
    uint32_t abi, struct_size;
    void (*log)(const char *message);
    void (*log_value)(const char *name, uint64_t value);
    uint32_t (*service_open)(const char *name, uint32_t *resource_id);
    uint32_t (*service_close)(uint32_t resource_id);
    uint32_t (*service_domain)(uint32_t resource_id);
    uint32_t (*ipc)(uint32_t resource_id, const ResearchIpc *request, ResearchReply *reply);
    uint32_t (*query_memory)(uint64_t address, ResearchMemoryInfo *info);
    uint32_t (*read_memory)(uint64_t address, void *output, uint32_t size);
    void (*sleep_ms)(uint32_t milliseconds);
    uint64_t (*time_ms)(void);
    uintptr_t (*resolve)(const char *sdk_symbol);
    uint32_t (*handle_close)(uint32_t handle); // Closes a raw handle returned by ipc().
} ResearchApi;
typedef struct {
    uint32_t abi, struct_size;
    const uint8_t *input;
    uint32_t input_size, reserved;
    uint8_t *output;
    uint32_t output_capacity, output_size;
    uint8_t *state; // Zeroed once on app launch; retained across all jobs.
    uint32_t state_size, reserved2;
} ResearchContext;
typedef uint64_t (*ResearchEntry)(const ResearchApi *, ResearchContext *);

// Implement this function. Use callbacks or resolve() for host functions.
uint64_t probe_entry(const ResearchApi *api, ResearchContext *context);
