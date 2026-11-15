// Read-only queries from the public Switch 2 23.0.1 scoped command inventory.
// Input: {u8 type, u8 mode, u16 zero, u32 guests}; default {0,0,0,1}.
#include "research_api.h"
typedef struct { uint8_t type, mode; uint16_t zero; uint32_t guests; } Config;
_Static_assert(sizeof(Config)==8,"wire size");
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    if (ctx->state_size<16 || ctx->output_capacity<40) return 1;
    uint32_t scoped=*(uint32_t *)(ctx->state+12);
    if (!scoped) { api->log("Create a scoped StreamPlay object first"); return 2; }
    Config config={0,0,0,1};
    if (ctx->input_size) {
        if (ctx->input_size!=8) return 3;
        __builtin_memcpy(&config,ctx->input,8);
    }
    if (config.type>1 || config.mode>1 || config.zero || config.guests<1 || config.guests>3) return 4;
    api->log_value("type",config.type); api->log_value("mode",config.mode);
    api->log_value("guests",config.guests);
    uint32_t memory=0; ResearchReply reply={0};
    ResearchIpc request={.command=1,.input_size=8,.input=&config,.output_size=4,.output=&memory};
    uint32_t rc=api->ipc(scoped,&request,&reply);
    api->log_value("memory_query_result",rc); if (rc) return rc;
    api->log_value("required_memory",memory);
    uint64_t sizes[4]={0}; config.guests=0;
    request=(ResearchIpc){.command=9,.input_size=8,.input=&config,.output_size=32,.output=sizes};
    rc=api->ipc(scoped,&request,&reply);
    api->log_value("work_sizes_result",rc); if (rc) return rc;
    api->log_value("global_size",sizes[0]); api->log_value("first_size",sizes[1]);
    api->log_value("second_size",sizes[2]); api->log_value("third_size",sizes[3]);
    __builtin_memcpy(ctx->output,&memory,4);
    __builtin_memset(ctx->output+4,0,4);
    __builtin_memcpy(ctx->output+8,sizes,32); ctx->output_size=40;
    return 0;
}
