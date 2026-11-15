#pragma once
#include <switch.h>
#include "research_api.h"
typedef Result (*RawSend)(Handle);
typedef Result (*RawClose)(Handle);
typedef struct {
    uint32_t copies[8], moves[8], copy_count, move_count;
    uint8_t data[128];
} RawReply;
static inline Result rawCall(const ResearchApi *api, Handle handle, uint32_t command,
    const void *input, uint32_t input_size, bool pid, RawReply *out, uint32_t out_size,
    void *buffer, size_t buffer_size, bool buffer_output) {
    RawSend send=(RawSend)api->resolve("svcSendSyncRequest");
    if (!send || input_size>128 || out_size>sizeof(out->data)) return 1;
    __builtin_memset(out,0,sizeof(*out));
    CmifRequest req=cmifMakeRequest(armGetTls(),(CmifRequestFormat){
        .request_id=command,.data_size=input_size,.send_pid=pid,
        .num_in_buffers=buffer && !buffer_output?1:0,.num_out_buffers=buffer && buffer_output?1:0});
    if (input_size) __builtin_memcpy(req.data,input,input_size);
    if (buffer) {
        if (buffer_output) cmifRequestOutBuffer(&req,buffer,buffer_size,HipcBufferMode_Normal);
        else cmifRequestInBuffer(&req,buffer,buffer_size,HipcBufferMode_Normal);
    }
    Result rc=send(handle); if (rc) return rc;
    CmifResponse response;
    rc=cmifParseResponse(&response,armGetTls(),false,out_size); if (rc) return rc;
    HipcResponse header=hipcParseResponse(armGetTls());
    if (header.num_copy_handles>8 || header.num_move_handles>8) return 2;
    out->copy_count=header.num_copy_handles; out->move_count=header.num_move_handles;
    for (uint32_t i=0;i<out->copy_count;i++) out->copies[i]=response.copy_handles[i];
    for (uint32_t i=0;i<out->move_count;i++) out->moves[i]=response.move_handles[i];
    if (out_size) __builtin_memcpy(out->data,response.data,out_size);
    return 0;
}
static inline Result rawOpen(const ResearchApi *api, Handle sm, const char *name, Handle *out) {
    SmServiceName encoded=smEncodeName(name); RawReply reply;
    Result rc=rawCall(api,sm,1,&encoded,8,false,&reply,0,NULL,0,false);
    if (!rc && reply.move_count!=1) return 3;
    if (!rc) *out=reply.moves[0];
    return rc;
}
