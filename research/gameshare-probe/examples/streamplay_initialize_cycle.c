// Diagnostic cycle: initialize, collect status locally, then finalize.
// Continues without a LAN link if the service switches wireless modes.
#define probe_entry status_after_init
#include "streamplay_status.c"
#undef probe_entry
typedef struct { uint8_t type,mode; uint16_t zero; uint32_t guests; uint64_t pid; } InitCycle;
_Static_assert(sizeof(InitCycle)==16,"wire size");
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    if (ctx->state_size<144) return 1;
    uint32_t scoped=*(uint32_t *)(ctx->state+12); if (!scoped) return 2;
    if (*(uint32_t *)(ctx->state+128)) return 3;
    InitCycle in={.guests=1};
    if (ctx->input_size) {
        if (ctx->input_size!=2 || ctx->input[0]>1 || ctx->input[1]>1) return 4;
        in.type=ctx->input[0]; in.mode=ctx->input[1];
    }
    ResearchIpc req={.command=2,.input_size=16,.input=&in,.send_pid=1};
    ResearchReply reply={0};
    api->log("Before Initialize (cmd 2); this may change wireless mode");
    uint32_t rc=api->ipc(scoped,&req,&reply);
    api->log_value("initialize_result",rc); if (rc) return rc;
    *(uint32_t *)(ctx->state+128)=1;
    uint64_t status=status_after_init(api,ctx);
    api->log_value("status_collection_result",status);
    api->log("Finalizing locally after diagnostic queries");
    req=(ResearchIpc){.command=3};
    rc=api->ipc(scoped,&req,&reply);
    api->log_value("finalize_result",rc);
    if (!rc) *(uint32_t *)(ctx->state+128)=0;
    if (!rc) {
        void (*reconnect)(void)=(void (*)(void))api->resolve("probeReconnectNetwork");
        if (reconnect) reconnect();
    }
    return rc;
}
