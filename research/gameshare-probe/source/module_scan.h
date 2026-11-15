// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROBE_MAX_MODULES 32
#define PROBE_MAX_SYMBOLS 512
#define PROBE_MAX_REGIONS 4096
typedef bool (*ProbeRead)(uint64_t address, void *output, size_t size, void *context);
typedef struct {
    char name[256];
    uint64_t address, size;
    uint16_t section;
    uint8_t info;
    bool executable;
} ProbeSymbol;
typedef struct {
    uint64_t base, dynamic;
    uint32_t symbol_count, matched_count;
    bool symbols_complete;
    char status[80];
} ProbeModule;
typedef struct {
    ProbeModule modules[PROBE_MAX_MODULES];
    ProbeSymbol symbols[PROBE_MAX_SYMBOLS];
    size_t module_count, symbol_count;
    uint32_t regions_visited;
    bool regions_complete, module_limit, symbol_limit;
    uint32_t query_result;
} ProbeInventory;

// Reads only validated ranges through the supplied reader; never calls symbols.
bool probeScanModule(ProbeInventory *inventory, uint64_t base,
                     ProbeRead read, void *context);
