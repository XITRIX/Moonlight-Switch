// Read-only CodeFS opening through the SDK's existing HEN FS proxy.
#include "raw_helpers.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    Handle fs=((uint32_t *)(ctx->state+64))[1]; if (!fs) return 1;
    uint32_t *owned=(uint32_t *)(ctx->state+96);
    if (owned[3]) { api->log_value("retained_chat_codefs",owned[3]); return 0; }
    struct { uint32_t type,padding; uint64_t program; } input={0,0,UINT64_C(0x040000000000004a)};
    RawReply reply;
    Result rc=rawCall(api,fs,7,&input,sizeof(input),false,&reply,0,NULL,0,false);
    api->log_value("FS_open_patched_Chat_CodeFS",rc);
    if (!rc && reply.move_count==1) { owned[3]=reply.moves[0]; api->log_value("chat_codefs",owned[3]); }
    return rc;
}
