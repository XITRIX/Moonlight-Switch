// Check whether a NEW loader session is available. Never touch a borrowed root.
#include "raw_helpers.h"
uint64_t probe_entry(const ResearchApi *api,ResearchContext *ctx) {
    Handle sm=((uint32_t *)(ctx->state+64))[2];if(!sm)return 1;
    RawClose close=(RawClose)api->resolve("svcCloseHandle");if(!close)return 2;
    Handle owned=0;Result rc=rawOpen(api,sm,"fsp-ldr",&owned);
    api->log_value("new_loader_session_result",rc);
    if(!rc) {
        if(!owned)return 3;
        api->log("New loader session is available; no CodeFS command sent");
        rc=close(owned);api->log_value("close_new_owned_loader_session",rc);
    }
    return rc;
}
