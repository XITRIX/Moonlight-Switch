// SPDX-License-Identifier: MIT
#include "agent_wire.h"
#include "research_api.h"
#include <string.h>
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
int agentParsePacket(const void *packet, size_t size, AgentPacket *out) {
    if (!packet || !out || size < AGENT_HEADER_SIZE) return 1;
    const uint8_t *p = packet;
    if (memcmp(p, "KJIT", 4) || le32(p+4) != RESEARCH_ABI || le32(p+20)) return 2;
    AgentPacket q = {.entry=le32(p+8), .code_size=le32(p+12), .input_size=le32(p+16)};
    if (!q.code_size || q.code_size > RESEARCH_CODE_LIMIT || q.input_size > RESEARCH_INPUT_LIMIT ||
        q.code_size%4 || q.entry%4 || q.entry >= q.code_size) return 3;
    if (size != AGENT_HEADER_SIZE+(size_t)q.code_size+q.input_size) return 4;
    *out = q; return 0;
}
