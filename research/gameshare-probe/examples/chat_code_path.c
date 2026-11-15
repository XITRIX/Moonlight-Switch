// Read-only NCM path lookup and CodeFS open, using the official libnx wire layouts.
#include "raw_helpers.h"
static Result pointerCall(const ResearchApi *api, Handle handle, uint32_t command,
    const void *input, uint32_t input_size, void *buffer, uint32_t size,
    bool output, RawReply *reply) {
    RawSend send=(RawSend)api->resolve("svcSendSyncRequest");
    CmifRequest req=cmifMakeRequest(armGetTls(),(CmifRequestFormat){
        .request_id=command,.data_size=input_size,.num_in_pointers=output?0:1,
        .num_out_fixed_pointers=output?1:0});
    if (input_size) __builtin_memcpy(req.data,input,input_size);
    if (output) cmifRequestOutFixedPointer(&req,buffer,size);
    else cmifRequestInPointer(&req,buffer,size);
    Result rc=send(handle); if (rc) return rc;
    CmifResponse response; rc=cmifParseResponse(&response,armGetTls(),false,0);
    if (rc) return rc;
    HipcResponse header=hipcParseResponse(armGetTls());
    if (header.num_move_handles>8 || header.num_copy_handles>8) return 2;
    __builtin_memset(reply,0,sizeof(*reply));
    reply->move_count=header.num_move_handles; reply->copy_count=header.num_copy_handles;
    for (uint32_t i=0;i<reply->move_count;i++) reply->moves[i]=response.move_handles[i];
    for (uint32_t i=0;i<reply->copy_count;i++) reply->copies[i]=response.copy_handles[i];
    return 0;
}
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    if (ctx->input_size!=16) return 1;
    Handle fs=((uint32_t *)(ctx->state+64))[1];
    uint32_t *owned=(uint32_t *)(ctx->state+96);
    if (!fs || !owned[0] || !owned[1]) return 2;
    if (owned[3]) { api->log_value("retained_chat_codefs",owned[3]); return 0; }
    RawReply reply;
    uint8_t key_input[24]={1}; // Program content type; seven bytes padding, meta key.
    __builtin_memcpy(key_input+8,ctx->input,16);
    Result rc=rawCall(api,owned[1],3,key_input,24,false,&reply,16,NULL,0,false);
    api->log_value("NCM_Chat_program_content_id",rc); if (rc) return rc;
    uint8_t content_id[16]; __builtin_memcpy(content_id,reply.data,16);
    __builtin_memcpy(ctx->output,content_id,16); ctx->output_size=16;
    uint8_t storage_id=3;
    rc=rawCall(api,owned[0],4,&storage_id,1,false,&reply,0,NULL,0,false);
    api->log_value("NCM_open_system_content_storage",rc); if (rc) return rc;
    if (reply.move_count!=1) return 3;
    Handle storage=reply.moves[0];
    char path[0x301]={0};
    rc=pointerCall(api,storage,8,content_id,16,path,0x300,true,&reply);
    api->log_value("NCM_get_Chat_content_path",rc);
    RawClose close=(RawClose)api->resolve("svcCloseHandle");
    close(storage);
    if (rc) return rc;
    api->log(path);
    __builtin_memcpy(ctx->output+16,path,sizeof(path)); ctx->output_size=16+sizeof(path);
    struct { uint8_t attr; uint8_t pad[3]; uint32_t type; uint64_t id; } in={0,{0},0,UINT64_C(0x040000000000004a)};
    rc=pointerCall(api,fs,10,&in,16,path,sizeof(path),false,&reply);
    api->log_value("FS_open_Chat_CodeFS_with_path",rc);
    if (!rc && reply.move_count==1) { owned[3]=reply.moves[0]; api->log_value("chat_codefs",owned[3]); }
    return rc;
}
