// Read-only process/module lookup. Does not attach, suspend or read target memory.
// libnx pm:dmnt GetProcessId=2 and ldr:dmnt GetProcessModuleInfo=2.
#include "raw_helpers.h"
static Result modules(const ResearchApi *api,Handle loader,uint64_t pid,
    ResearchContext *ctx) {
    RawSend send=(RawSend)api->resolve("svcSendSyncRequest");
    if(!send || ctx->output_capacity<16+16*sizeof(LoaderModuleInfo))return 1;
    LoaderModuleInfo *items=(void *)(ctx->output+16);
    __builtin_memset(ctx->output,0,16+16*sizeof(*items));
    CmifRequest req=cmifMakeRequest(armGetTls(),(CmifRequestFormat){
        .request_id=2,.data_size=8,.num_out_pointers=1});
    __builtin_memcpy(req.data,&pid,8);
    cmifRequestOutPointer(&req,items,16*sizeof(*items));
    Result rc=send(loader);if(rc)return rc;
    CmifResponse response;rc=cmifParseResponse(&response,armGetTls(),false,4);
    if(rc)return rc;
    int32_t count;__builtin_memcpy(&count,response.data,4);
    if(count<0 || count>16)return 2;
    api->log_value("Chat_module_count",(uint32_t)count);
    __builtin_memcpy(ctx->output,&pid,8);__builtin_memcpy(ctx->output+8,&count,4);
    ctx->output_size=16+(uint32_t)count*sizeof(*items);
    for(int32_t i=0;i<count;i++) {
        api->log_value("Chat_module_base",items[i].base_address);
        api->log_value("Chat_module_size",items[i].size);
    }
    return 0;
}
uint64_t probe_entry(const ResearchApi *api,ResearchContext *ctx) {
    uint32_t ids[2]={0};const char *names[]={"pm:dmnt","ldr:dmnt"};
    for(uint32_t i=0;i<2;i++) {
        api->log(names[i]);Result rc=api->service_open(names[i],&ids[i]);
        api->log_value("normal_SM_open_result",rc);
        if(!rc)api->log_value("normal_SM_close_result",api->service_close(ids[i]));
    }
    Handle sm=((uint32_t *)(ctx->state+64))[2];if(!sm)return 3;
    RawClose close=(RawClose)api->resolve("svcCloseHandle");if(!close)return 4;
    Handle pm=0,loader=0;RawReply reply;uint64_t pid=0;
    Result rc=rawOpen(api,sm,"pm:dmnt",&pm);
    api->log_value("FS_identity_open_pm_dmnt",rc);if(rc)return rc;
    uint64_t title=UINT64_C(0x040000000000004a);
    rc=rawCall(api,pm,2,&title,8,false,&reply,8,NULL,0,false);
    api->log_value("Chat_PID_result",rc);
    if(!rc)__builtin_memcpy(&pid,reply.data,8);
    api->log_value("close_owned_pm_dmnt",close(pm));if(rc)return rc;
    api->log_value("Chat_PID",pid);if(!pid)return 5;
    rc=rawOpen(api,sm,"ldr:dmnt",&loader);
    api->log_value("FS_identity_open_ldr_dmnt",rc);if(rc)return rc;
    rc=modules(api,loader,pid,ctx);
    api->log_value("Chat_module_info_result",rc);
    api->log_value("close_owned_ldr_dmnt",close(loader));
    return rc;
}
