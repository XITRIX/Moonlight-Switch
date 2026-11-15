// Read only this application's ARP property using the existing HEN SM proxy.
#include "raw_helpers.h"
uint64_t probe_entry(const ResearchApi *api,ResearchContext *ctx) {
    if(ctx->state_size<80 || ctx->output_capacity<sizeof(NacpStruct))return 1;
    Handle proxy=((uint32_t *)(ctx->state+64))[2];if(!proxy)return 2;
    u64 (*aruid)(void)=(void*)api->resolve("appletGetAppletResourceUserId");
    RawClose close=(RawClose)api->resolve("svcCloseHandle");if(!aruid||!close)return 3;
    Handle reader=0;Result rc=rawOpen(api,proxy,"arp:r",&reader);
    api->log_value("proxy_arp_reader_open",rc);if(rc)return rc;
    uint64_t id=aruid();RawReply reply;
    rc=rawCall(api,reader,3,&id,8,false,&reply,8,0,0,false);
    api->log_value("get_application_instance_id",rc);
    if(rc) { close(reader);return rc; }
    __builtin_memcpy(&id,reply.data,8);
    api->log_value("application_instance_id",id);
    __builtin_memset(ctx->output,0,sizeof(NacpStruct));
    rc=rawCall(api,reader,1,&id,8,false,&reply,0,ctx->output,sizeof(NacpStruct),true);
    api->log_value("get_current_control_property",rc);
    if(!rc)ctx->output_size=sizeof(NacpStruct);
    api->log_value("close_own_arp_reader",close(reader));
    return rc;
}
