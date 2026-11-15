// Uses the SDK's wire builders without serviceCreate's implicit CMIF control query.
// Request only the documented scoped-object creator; no guessed object methods.
#include <switch.h>
#include "research_api.h"
typedef Result (*SendSync)(Handle);
typedef Result (*Close)(Handle);
typedef struct { uint64_t open_offset, sm_offset; uint8_t expected[16]; uint32_t transport; } Layout;
uint64_t probe_entry(const ResearchApi *api, ResearchContext *context) {
    SendSync send=(SendSync)api->resolve("svcSendSyncRequest");
    Close close=(Close)api->resolve("svcCloseHandle");
    if (!send || !close) return 1;
    if (context->input_size<sizeof(Layout)) return 2;
    Layout local; __builtin_memcpy(&local,context->input,sizeof(local));
    const Layout *layout=&local;
    uint64_t open=(uintptr_t)api->service_open;
    if (open<layout->open_offset) return 3;
    uint64_t base=open-layout->open_offset;
    uint8_t checked[16];
    Result rc=api->read_memory(open,checked,16); if (rc) return rc;
    for (size_t i=0;i<16;i++) if (checked[i]!=layout->expected[i]) return 4;
    Service borrowed={0};
    rc=api->read_memory(base+layout->sm_offset,&borrowed,sizeof(borrowed));
    if (rc || !borrowed.session) return rc?rc:5;
    api->log_value("verified_agent_base",base);
    api->log_value("borrowed_registered_SM",borrowed.session);
    Handle sm=borrowed.session, root=0;
    if (((uint32_t *)(context->state+32))[0]) return 6;
    CmifRequest req;
    CmifResponse reply={0};
    if (!rc) {
        req=cmifMakeRequest(armGetTls(),(CmifRequestFormat){.request_id=1,.data_size=8});
        SmServiceName name=smEncodeName("stpl:u"); __builtin_memcpy(req.data,&name,8);
        rc=send(sm);
        if (!rc) rc=cmifParseResponse(&reply,armGetTls(),false,0);
        if (!rc) root=reply.move_handles[0];
        api->log_value("get_raw_stpl",rc); api->log_value("root_handle",root);
    }
    if (!rc) {
        // Native Switch 2 services commonly expose TIPC; the zero-input creator
        // keeps the documented command and input unchanged while checking transport.
        if (layout->transport==1) hipcMakeRequestInline(armGetTls(),.type=16);
        else {
            req=cmifMakeRequest(armGetTls(),(CmifRequestFormat){.request_id=0,.data_size=layout->transport==2?4:0});
            // Firmware 23's documented versioned StreamPlay creator is a
            // bounded compatibility hypothesis after the older no-input ABI failed.
            if (layout->transport==2) { *(uint32_t *)req.data=0x30000; api->log_value("creator_version",0x30000); }
        }
        rc=send(root); api->log_value("creator_kernel_result",rc);
        if (!rc) {
            __builtin_memcpy(context->output,armGetTls(),256); context->output_size=256;
            HipcResponse response=hipcParseResponse(armGetTls());
            api->log_value("reply_data_words",response.num_data_words);
            api->log_value("reply_move_handles",response.num_move_handles);
            if (layout->transport==1 && response.num_data_words) {
                rc=response.data_words[0]; api->log_value("TIPC_service_result",rc);
            } else if (layout->transport!=1) {
                rc=cmifParseResponse(&reply,armGetTls(),false,0);
                api->log_value("CMIF_service_result",rc);
            }
            if (!rc && response.num_move_handles==1) {
                Handle scoped=response.move_handles[0];
                // Keep raw root and scoped handles in a separate state namespace.
                ((uint32_t *)(context->state+32))[0]=root;
                ((uint32_t *)(context->state+32))[1]=scoped;
                api->log_value("scoped_raw_handle",scoped);
                root=0;
            } else {
                for (uint32_t i=0;i<response.num_move_handles && i<8;i++) close(response.move_handles[i]);
                for (uint32_t i=0;i<response.num_copy_handles && i<8;i++) close(response.copy_handles[i]);
            }
        }
    }
    if (root) close(root);
    // Borrowed SM belongs to the agent and is neither reinitialized nor closed.
    return rc;
}
