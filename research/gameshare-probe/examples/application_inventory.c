// Read-only metadata inventory; no application launches or storage writes.
#include "raw_helpers.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    Handle root=((uint32_t *)(ctx->state+96))[0]; if (!root) return 1;
    RawClose close=(RawClose)api->resolve("svcCloseHandle"); if (!close) return 2;
    const uint8_t storages[]={4,5,2}, types[]={0x80,0x81};
    uint32_t written=0; RawReply reply;
    for (size_t s=0;s<sizeof(storages);s++) {
        uint8_t storage=storages[s];
        Result rc=rawCall(api,root,5,&storage,1,false,&reply,0,NULL,0,false);
        api->log_value("storage_id",storage); api->log_value("open_database",rc);
        if (rc) continue;
        if (reply.move_count!=1) return 3;
        Handle db=reply.moves[0];
        for (size_t t=0;t<sizeof(types);t++) {
            uint8_t in[32]={0}; in[0]=types[t];
            for (size_t i=24;i<32;i++) in[i]=255;
            uint8_t *keys=ctx->state+24576;
            rc=rawCall(api,db,5,in,32,false,&reply,8,keys,8192,true);
            api->log_value("meta_type",types[t]); api->log_value("list_result",rc);
            if (rc) continue;
            int32_t counts[2]; __builtin_memcpy(counts,reply.data,8);
            if (counts[0]<0 || counts[1]<0 || counts[1]>512 || counts[1]>counts[0]) { close(db); return 4; }
            api->log_value("total",counts[0]); api->log_value("written",counts[1]);
            uint32_t bytes=12+(uint32_t)counts[1]*16;
            if (bytes>ctx->output_capacity-written) { close(db); return 5; }
            uint8_t header[4]={storage,types[t],0,0};
            __builtin_memcpy(ctx->output+written,header,4);
            __builtin_memcpy(ctx->output+written+4,counts,8);
            __builtin_memcpy(ctx->output+written+12,keys,(size_t)counts[1]*16);
            written+=bytes; ctx->output_size=written;
        }
        close(db);
    }
    return 0;
}
