// Open the SDK's documented HEN homebrew session and inspect its read-only facilities.
#include <switch.h>
#include "research_api.h"
typedef struct {
    uint64_t open_offset, sm_offset; uint8_t open_code[16]; uint32_t transport, reserved;
    uint64_t override_offset; uint8_t override_code[16];
} Layout;
typedef Result (*Send)(Handle);
typedef Handle (*GetOverride)(SmServiceName);
static Result call(Send send, Handle handle, uint32_t command, const void *input,
                   uint32_t input_size, bool pid, CmifResponse *response, uint32_t output_size) {
    CmifRequest request=cmifMakeRequest(armGetTls(),(CmifRequestFormat){
        .request_id=command,.data_size=input_size,.send_pid=pid});
    if (input_size) __builtin_memcpy(request.data,input,input_size);
    Result rc=send(handle);
    return rc?rc:cmifParseResponse(response,armGetTls(),false,output_size);
}
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    if (ctx->input_size<sizeof(Layout)) return 1;
    Layout layout; __builtin_memcpy(&layout,ctx->input,sizeof(layout));
    uint64_t open=(uintptr_t)api->service_open;
    if (open<layout.open_offset) return 2;
    uint64_t base=open-layout.open_offset;
    uint8_t checked[16];
    Result rc=api->read_memory(open,checked,16); if (rc) return rc;
    for (size_t i=0;i<16;i++) if (checked[i]!=layout.open_code[i]) return 3;
    rc=api->read_memory(base+layout.override_offset,checked,16); if (rc) return rc;
    for (size_t i=0;i<16;i++) if (checked[i]!=layout.override_code[i]) return 4;
    Send send=(Send)api->resolve("svcSendSyncRequest"); if (!send) return 5;
    GetOverride get=(GetOverride)(uintptr_t)(base+layout.override_offset);
    Handle ro=get(smEncodeName("ldr:ro")); api->log_value("borrowed_RO",ro); if (!ro) return 6;
    uint32_t *owned=(uint32_t *)(ctx->state+64);
    if (owned[0]) { api->log("HEN handles already retained"); return 7; }
    uint64_t pid_reserved=0; CmifResponse response={0};
    rc=call(send,ro,65004,&pid_reserved,8,true,&response,0);
    api->log_value("RO_get_HEN_session",rc); if (rc) return rc;
    HipcResponse header=hipcParseResponse(armGetTls());
    if (header.num_copy_handles!=1) return 8;
    owned[0]=response.copy_handles[0];
    rc=call(send,owned[0],0,NULL,0,false,&response,16);
    api->log_value("HEN_get_status",rc); if (rc) return rc;
    struct { uint64_t pid; uint32_t version, initialized; } status;
    __builtin_memcpy(&status,response.data,16);
    api->log_value("HEN_pid",status.pid); api->log_value("HEN_protocol",status.version);
    api->log_value("HEN_initialization_result",status.initialized);
    if (status.version!=6 || status.initialized) return 9;
    rc=call(send,owned[0],16,NULL,0,false,&response,0);
    api->log_value("HEN_get_FS_proxy",rc);
    if (!rc) { header=hipcParseResponse(armGetTls()); if (header.num_copy_handles==1) owned[1]=response.copy_handles[0]; }
    if (rc) return rc;
    rc=call(send,owned[0],19,NULL,0,false,&response,0);
    api->log_value("HEN_get_FS_identity_SM",rc);
    if (!rc) { header=hipcParseResponse(armGetTls()); if (header.num_copy_handles==1) owned[2]=response.copy_handles[0]; }
    api->log_value("owned_HEN_handle",owned[0]); api->log_value("owned_FS_handle",owned[1]);
    api->log_value("borrowed_identity_SM_copy",owned[2]);
    // The SM copy must only be kernel-closed later: never send Initialize or Close to it.
    return rc;
}
