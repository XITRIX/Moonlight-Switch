// Read status and duplicate/close event/HID handles; does not map or alter HID.
#include "research_api.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    if (ctx->state_size<144 || ctx->output_capacity<64) return 1;
    uint32_t scoped=*(uint32_t *)(ctx->state+12); if (!scoped) return 2;
    ResearchReply reply={0}; uint64_t data[2]={0};
    uint32_t commands[]={8,502,503,7,300};
    for (uint32_t i=0;i<5;i++) {
        uint32_t cmd=commands[i]; data[0]=data[1]=0;
        ResearchIpc req={.command=cmd,.output=data,.output_size=cmd==8?8:cmd==300?16:cmd==7?0:1};
        if (cmd==7 || cmd==300) req.output_handle_attributes[0]=1;
        uint32_t rc=api->ipc(scoped,&req,&reply);
        api->log_value("command",cmd); api->log_value("result",rc);
        if (!rc) {
            api->log_value("data0",data[0]); api->log_value("data1",data[1]);
            if (cmd==7 || cmd==300) {
                api->log_value("returned_handle",reply.handles[0]);
                if (reply.handles[0]) api->log_value("close_handle_result",api->handle_close(reply.handles[0]));
            }
        }
        // Read queries may report uninitialized; retain all results instead of guessing.
        __builtin_memcpy(ctx->output+i*12,&cmd,4);
        __builtin_memcpy(ctx->output+i*12+4,&rc,4);
        __builtin_memcpy(ctx->output+i*12+8,data,4);
    }
    ctx->output_size=60;
    return 0;
}
