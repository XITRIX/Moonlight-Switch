// Firmware 23.0.1: command 0 takes u32 0x30000 (verified on the console).
// Older no-input documentation did not match this firmware. This only creates
// the scoped interface; it does not initialize or start GameShare hosting.
#include "research_api.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *context) {
    uint32_t *root=(uint32_t *)(context->state+8);
    uint32_t *scoped=(uint32_t *)(context->state+12);
    if (!*root) { api->log("Run open_streamplay.c first"); return 1; }
    if (*scoped) { api->log_value("existing_scoped_id",*scoped); return 0; }
    uint32_t version=0x30000;
    if(context->input_size) {
        if(context->input_size!=4)return 2;
        __builtin_memcpy(&version,context->input,4);
        if(version!=0x30000 && version!=0x12)return 3;
    }
    api->log_value("factory_compatibility_parameter",version);
    ResearchIpc request={.command=0,.input_size=sizeof(version),.input=&version,.output_object_count=1};
    ResearchReply reply={0};
    uint32_t result=api->ipc(*root,&request,&reply);
    api->log_value("creator_result",result);
    if (!result) *scoped=reply.objects[0];
    api->log_value("scoped_id",*scoped);
    return result;
}
