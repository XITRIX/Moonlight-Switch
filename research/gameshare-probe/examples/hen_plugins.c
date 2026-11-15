// Inspect only the documented HEN plugin catalog; do not open or run plugins.
#include "raw_helpers.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    Handle hen=((uint32_t *)(ctx->state+64))[0]; if (!hen) return 1;
    RawReply reply;
    Result rc=rawCall(api,hen,20,NULL,0,false,&reply,4,NULL,0,false);
    api->log_value("HEN_GetPluginCount",rc); if (rc) return rc;
    uint32_t count; __builtin_memcpy(&count,reply.data,4);
    api->log_value("published_plugins",count); if (count>16) return 2;
    for (uint32_t i=0;i<count;i++) {
        rc=rawCall(api,hen,21,&i,4,false,&reply,64,NULL,0,false);
        api->log_value("HEN_GetPluginName",rc); if (rc) return rc;
        char name[65]; __builtin_memcpy(name,reply.data,64); name[64]=0;
        api->log(name);
    }
    return 0;
}
