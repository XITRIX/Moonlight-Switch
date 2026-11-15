// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#define AGENT_HEADER_SIZE 24
typedef struct { uint32_t entry, code_size, input_size; } AgentPacket;
// KJIT + little endian u32: ABI, entry offset, code bytes, input bytes, reserved=0.
int agentParsePacket(const void *packet, size_t size, AgentPacket *out);
