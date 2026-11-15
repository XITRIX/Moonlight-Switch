// Read only this application's registered control property through arp:r.
// Switch 2 23.0.1 Glue inventory: reader cmd1, u64 ARUID, output buffer.
#include <switch.h>
#include "research_api.h"
uint64_t probe_entry(const ResearchApi *api,ResearchContext *ctx) {
    if(ctx->output_capacity<sizeof(NacpStruct))return 1;
    u64 (*aruid)(void)=(void*)api->resolve("appletGetAppletResourceUserId");if(!aruid)return 2;
    uint32_t reader=0,rc=api->service_open("arp:r",&reader);
    api->log_value("arp_reader_open_result",rc);if(rc)return rc;
    uint64_t id=aruid();__builtin_memset(ctx->output,0,sizeof(NacpStruct));
    ResearchIpc req={.command=1,.input=&id,.input_size=8,.buffer_count=1,
        .buffers={{.attributes=SfBufferAttr_Out|SfBufferAttr_HipcMapAlias,.size=sizeof(NacpStruct),.data=ctx->output}}};
    ResearchReply reply={0};rc=api->ipc(reader,&req,&reply);
    api->log_value("get_control_property_result",rc);
    if(!rc)ctx->output_size=sizeof(NacpStruct);
    api->log_value("close_arp_reader_result",api->service_close(reader));
    return rc;
}
