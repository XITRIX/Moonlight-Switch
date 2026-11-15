// Bounded read-only NCM inventory through the existing HEN FS-identity SM copy.
#include "raw_helpers.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    Handle sm=((uint32_t *)(ctx->state+64))[2]; if (!sm) return 1;
    uint32_t *owned=(uint32_t *)(ctx->state+96);
    Result rc=0; RawReply reply;
    if (!owned[0]) {
        rc=rawOpen(api,sm,"ncm",&owned[0]); api->log_value("open_ncm",rc); if (rc) return rc;
    }
    if (!owned[1]) {
        uint8_t storage=3;
        rc=rawCall(api,owned[0],5,&storage,1,false,&reply,0,NULL,0,false);
        api->log_value("NCM_open_BuiltInSystem_database",rc); if (rc) return rc;
        if (reply.move_count!=1) return 2;
        owned[1]=reply.moves[0];
    }
    uint8_t input[32]={1,0};
    for (size_t i=24;i<32;i++) input[i]=255;
    uint8_t *keys=ctx->state+4096;
    rc=rawCall(api,owned[1],5,input,sizeof(input),false,&reply,8,keys,16384,true);
    api->log_value("NCM_list_SystemProgram",rc); if (rc) return rc;
    int32_t counts[2]; __builtin_memcpy(counts,reply.data,8);
    if (counts[0]<0 || counts[1]<0 || counts[1]>1024 || counts[1]>counts[0]) return 3;
    api->log_value("total",counts[0]); api->log_value("written",counts[1]);
    __builtin_memcpy(ctx->output,counts,8); __builtin_memcpy(ctx->output+8,keys,(size_t)counts[1]*16);
    ctx->output_size=8+(uint32_t)counts[1]*16;
    if (!owned[2]) {
        rc=rawOpen(api,sm,"fsp-ldr",&owned[2]); api->log_value("open_fsp_ldr",rc);
    }
    return rc;
}
