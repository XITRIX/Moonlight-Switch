// SPDX-License-Identifier: MIT
#include "../source/agent_wire.h"
#include "../source/research_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put(uint8_t *p, uint32_t x) {
    p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); p[2]=(uint8_t)(x>>16); p[3]=(uint8_t)(x>>24);
}
int main(void) {
    size_t maximum=AGENT_HEADER_SIZE+RESEARCH_CODE_LIMIT+RESEARCH_INPUT_LIMIT;
    uint8_t *bytes=calloc(1,maximum); assert(bytes);
    memcpy(bytes,"KJIT",4); put(bytes+4,1); put(bytes+12,8);
    AgentPacket packet;
    assert(!agentParsePacket(bytes,32,&packet));
    assert(packet.code_size==8 && packet.entry==0 && packet.input_size==0);
    assert(agentParsePacket(bytes,23,&packet));
    assert(agentParsePacket(bytes,31,&packet));
    assert(agentParsePacket(bytes,33,&packet));
    put(bytes+8,4); assert(!agentParsePacket(bytes,32,&packet));
    put(bytes+8,8); assert(agentParsePacket(bytes,32,&packet));
    put(bytes+8,1); assert(agentParsePacket(bytes,32,&packet));
    put(bytes+8,0); put(bytes+20,1); assert(agentParsePacket(bytes,32,&packet));
    put(bytes+20,0); put(bytes+4,2); assert(agentParsePacket(bytes,32,&packet));
    put(bytes+4,1); put(bytes+12,RESEARCH_CODE_LIMIT); put(bytes+16,RESEARCH_INPUT_LIMIT);
    assert(!agentParsePacket(bytes,maximum,&packet));
    put(bytes+16,RESEARCH_INPUT_LIMIT+1); assert(agentParsePacket(bytes,maximum,&packet));
    put(bytes+16,0); put(bytes+12,UINT32_MAX); assert(agentParsePacket(bytes,maximum,&packet));
    free(bytes);
    puts("agent framing: complete packets, limits, truncation, ABI, reserved flags and alignment passed");
    return 0;
}
