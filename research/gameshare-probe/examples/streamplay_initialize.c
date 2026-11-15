// Initialize one scoped StreamPlay session, without opening matchmaking.
// Layout hypothesis: Recompia proxy analysis + exact 23.0.1 16-byte/PID inventory.
#include "research_api.h"
typedef struct { uint8_t type,mode; uint16_t zero; uint32_t guests; uint64_t pid; } Init;
_Static_assert(sizeof(Init)==16,"wire size");
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    if (ctx->state_size<144) return 1;
    uint32_t scoped=*(uint32_t *)(ctx->state+12);
    uint32_t *initialized=(uint32_t *)(ctx->state+128);
    if (!scoped || *initialized) return 2;
    Init in={.guests=1};
    if (ctx->input_size) {
        if (ctx->input_size!=2) return 3;
        in.type=ctx->input[0]; in.mode=ctx->input[1];
    }
    if (in.type>1 || in.mode>1) return 4;
    ResearchIpc request={.command=2,.input_size=16,.input=&in,.send_pid=1};
    ResearchReply reply={0};
    uint32_t rc=api->ipc(scoped,&request,&reply);
    api->log_value("initialize_result",rc);
    if (rc) return rc;
    *initialized=1;
    ctx->state[132]=in.type; ctx->state[133]=in.mode;
    api->log("StreamPlay initialized; no matchmaking/session advertisement yet");
    return 0;
}
