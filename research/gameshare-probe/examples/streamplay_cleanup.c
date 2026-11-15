// Release only the diagnostic session handles and idle StreamPlay resources.
#define probe_entry close_idle_streamplay
#include "close_streamplay.c"
#undef probe_entry
#include <switch.h>
uint64_t probe_entry(const ResearchApi *api,ResearchContext *ctx) {
    if(ctx->state_size<800)return 1;
    if(*(uint32_t *)(ctx->state+128) || *(uint64_t *)(ctx->state+264)) {
        api->log("Refusing cleanup while initialization/work memory is retained");return 2;
    }
    Result (*close)(Handle)=(void*)api->resolve("svcCloseHandle");if(!close)return 3;
    uint32_t *handles=(uint32_t *)(ctx->state+64);uint32_t failed=0;
    for(uint32_t i=0;i<3;i++)if(handles[i]) {
        uint32_t rc=close(handles[i]);api->log_value("close_owned_diagnostic_handle",rc);
        if(!rc)handles[i]=0;else failed=rc;
    }
    uint64_t rc=close_idle_streamplay(api,ctx);
    if(!rc && !failed)api->log("All test work memory finalized/freed; diagnostic handles released");
    return failed?failed:rc;
}
