// SPDX-License-Identifier: MIT
// OunceCake-style CodeFS/file access over the existing persistent research ABI.
// No console filesystem writes. State namespace 16384 is private to this adapter.
#include "raw_helpers.h"
#include "../research-backend/kaihen/common/research_codefs.h"
typedef struct {
    uint32_t magic,abi,op,storage,attributes,reserved;
    uint64_t program_id,offset;
    uint32_t length,reserved2;
    char filename[16];
} ExtractRequest;
_Static_assert(sizeof(ExtractRequest)==64,"request wire layout");
typedef struct { uint32_t magic,code,file,reserved; uint64_t size; } ExtractState;
static Result pointerCall(const ResearchApi* api, Handle handle, uint32_t cmd,
    const void* input, uint32_t size, const char* path, RawReply* out) {
    RawSend send=(RawSend)api->resolve("svcSendSyncRequest");
    CmifRequest req=cmifMakeRequest(armGetTls(),(CmifRequestFormat){
        .request_id=cmd,.data_size=size,.num_in_pointers=1});
    __builtin_memcpy(req.data,input,size); cmifRequestInPointer(&req,path,0x301);
    Result rc=send(handle); if (rc) return rc;
    CmifResponse response;rc=cmifParseResponse(&response,armGetTls(),false,0);if(rc)return rc;
    HipcResponse header=hipcParseResponse(armGetTls());
    __builtin_memset(out,0,sizeof(*out));
    if (header.num_copy_handles || header.num_move_handles!=1) return 0x1759;
    out->move_count=1;out->moves[0]=response.move_handles[0];return 0;
}
static void cleanup(const ResearchApi* api, ExtractState* state) {
    RawClose close=(RawClose)api->resolve("svcCloseHandle");
    if (state->file) close(state->file);
    if (state->code) close(state->code);
    __builtin_memset(state,0,sizeof(*state));
}
uint64_t probe_entry(const ResearchApi* api, ResearchContext* ctx) {
    if (ctx->input_size!=sizeof(ExtractRequest)) return 0x1759;
    ExtractRequest in;__builtin_memcpy(&in,ctx->input,sizeof(in));
    if(in.magic!=0x314b4453 || in.abi!=1 || in.reserved || in.reserved2)return 0x1759;
    if(ctx->state_size<16416)return 0x1759;
    ExtractState* state=(ExtractState*)(ctx->state+16384);
    if(state->magic && state->magic!=0x314b4453)return 0x1759;
    if(in.op==4){cleanup(api,state);return 0;}
    Handle hen=((uint32_t*)(ctx->state+64))[0];
    if(!hen){api->log("Initialize HEN with hen_readonly.c first");return 0x1759;}
    RawReply reply;Result rc;
    if(in.op==0){ // Read the inserted-card Application metadata.
        Handle sm=((uint32_t*)(ctx->state+64))[2],ncm=0,db=0;
        if(!sm)return 0x1759;
        RawClose close=(RawClose)api->resolve("svcCloseHandle");
        rc=rawOpen(api,sm,"ncm",&ncm);if(rc)return rc;
        uint8_t storage=2;
        rc=rawCall(api,ncm,5,&storage,1,false,&reply,0,NULL,0,false);
        if(!rc && reply.move_count==1)db=reply.moves[0];
        else {close(ncm);return rc?rc:0x1759;}
        uint8_t input[32]={0x80};for(unsigned i=24;i<32;i++)input[i]=255;
        rc=rawCall(api,db,5,input,32,false,&reply,8,ctx->output+8,8192,true);
        if(!rc){
            int32_t counts[2];__builtin_memcpy(counts,reply.data,8);
            if(counts[0]<0 || counts[1]<0 || counts[1]>512 || counts[1]>counts[0])rc=0x1759;
            else{__builtin_memcpy(ctx->output,counts,8);ctx->output_size=8+(uint32_t)counts[1]*16;}
        }
        close(db);close(ncm);return rc;
    }
    if(in.op==1 || in.op==2){
        rc=rawCall(api,hen,ResearchCodeFs_Info,NULL,0,false,&reply,16,NULL,0,false);
        api->log_value("research_backend_info",rc);if(rc)return rc;
        ResearchCodeFsInfo info;__builtin_memcpy(&info,reply.data,16);
        if(info.magic!=ResearchCodeFs_Magic || info.abi!=1 || info.reserved)return 0x1759;
        __builtin_memcpy(ctx->output,&info,16);ctx->output_size=16;
        if(in.op==1)return 0;
        if(!info.loader_available)return 0x1759;
        if(state->file || state->code){api->log("Close the previous extraction first");return 0x1759;}
        if(in.storage!=2 || in.attributes>1)return 0x1759;
        if(__builtin_memcmp(in.filename,"sdk\0",4) && __builtin_memcmp(in.filename,"main\0",5))return 0x1759;
        ResearchCodeFsOpen open={(uint8_t)in.attributes,2,{0},in.program_id};
        if(!researchCodeFsValid(&open))return 0x1759;
        rc=rawCall(api,hen,ResearchCodeFs_Open,&open,16,false,&reply,0,NULL,0,false);
        api->log_value("open_cartridge_CodeFS",rc);if(rc)return rc;
        if(reply.copy_count!=1 || reply.move_count)return 0x1759;
        state->magic=0x314b4453;state->code=reply.copies[0];
        char path[0x301]={0};path[0]='/';
        for(unsigned i=0;i<15 && in.filename[i];i++)path[i+1]=in.filename[i];
        const uint32_t read_mode=1;
        rc=pointerCall(api,state->code,8,&read_mode,4,path,&reply);
        if(rc){cleanup(api,state);return rc;}
        state->file=reply.moves[0];
        rc=rawCall(api,state->file,4,NULL,0,false,&reply,8,NULL,0,false);
        if(rc){cleanup(api,state);return rc;}
        __builtin_memcpy(&state->size,reply.data,8);
        if(!state->size || state->size>256*1024*1024){cleanup(api,state);return 0x1759;}
        __builtin_memcpy(ctx->output,&state->size,8);ctx->output_size=8;return 0;
    }
    if(in.op==3){
        if(!state->file || !in.length || in.length>65536 ||
            in.offset>state->size || in.length>state->size-in.offset)return 0x1759;
        struct{uint32_t option,pad;uint64_t offset,size;} args={0,0,in.offset,in.length};
        RawSend send=(RawSend)api->resolve("svcSendSyncRequest");
        CmifRequest req=cmifMakeRequest(armGetTls(),(CmifRequestFormat){
            .request_id=0,.data_size=sizeof(args),.num_out_buffers=1});
        __builtin_memcpy(req.data,&args,sizeof(args));
        cmifRequestOutBuffer(&req,ctx->output,in.length,HipcBufferMode_NonSecure);
        rc=send(state->file);if(rc)return rc;
        CmifResponse response;rc=cmifParseResponse(&response,armGetTls(),false,8);if(rc)return rc;
        uint64_t got;__builtin_memcpy(&got,response.data,8);
        if(got!=in.length)return 0x1759;
        ctx->output_size=(uint32_t)got;return 0;
    }
    return 0x1759;
}
