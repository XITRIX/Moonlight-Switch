// Release this probe's initialized scoped StreamPlay service, no HEN/RO writes.
#include "research_api.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    if (ctx->state_size<144) return 1;
    uint32_t scoped=*(uint32_t *)(ctx->state+12); if (!scoped) return 2;
    if (!*(uint32_t *)(ctx->state+128)) return 3;
    ResearchIpc req={.command=3}; ResearchReply reply={0};
    uint32_t rc=api->ipc(scoped,&req,&reply);
    api->log_value("finalize_result",rc);
    if (!rc) *(uint32_t *)(ctx->state+128)=0;
    if (!rc) {
        void (*reconnect)(void)=(void (*)(void))api->resolve("probeReconnectNetwork");
        if (reconnect) reconnect();
    }
    return rc;
}
