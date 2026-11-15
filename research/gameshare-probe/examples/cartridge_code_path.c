// Read-only alternate CodeFS path using native content attribute 1.
// Unlike fsp-ldr this uses the already held FS proxy and installed NCM records.
#include "raw_helpers.h"
static Result pointerCall(const ResearchApi* api,Handle handle,uint32_t command,
    const void* input,uint32_t size,void* path,uint32_t path_size,bool output,RawReply* out) {
    RawSend send=(RawSend)api->resolve("svcSendSyncRequest");
    CmifRequest req=cmifMakeRequest(armGetTls(),(CmifRequestFormat){
        .request_id=command,.data_size=size,.num_in_pointers=output?0:1,
        .num_out_fixed_pointers=output?1:0});
    __builtin_memcpy(req.data,input,size);
    if(output)cmifRequestOutFixedPointer(&req,path,path_size);else cmifRequestInPointer(&req,path,path_size);
    Result rc=send(handle);if(rc)return rc;
    CmifResponse res;rc=cmifParseResponse(&res,armGetTls(),false,0);if(rc)return rc;
    HipcResponse header=hipcParseResponse(armGetTls());__builtin_memset(out,0,sizeof(*out));
    if(header.num_move_handles>1 || header.num_copy_handles)return 0x1759;
    out->move_count=header.num_move_handles;if(out->move_count)out->moves[0]=res.move_handles[0];return 0;
}
uint64_t probe_entry(const ResearchApi* api,ResearchContext* ctx) {
    if(ctx->input_size!=16)return 1;
    Handle fs=((uint32_t*)(ctx->state+64))[1],ncm=((uint32_t*)(ctx->state+96))[0];
    if(!fs || !ncm)return 2;
    RawClose close=(RawClose)api->resolve("svcCloseHandle");RawReply out;Result rc;
    uint8_t storage_id=2;
    rc=rawCall(api,ncm,5,&storage_id,1,false,&out,0,NULL,0,false);if(rc)return rc;
    if(out.move_count!=1)return 3;
    Handle db=out.moves[0];uint8_t key[24]={1};__builtin_memcpy(key+8,ctx->input,16);
    rc=rawCall(api,db,3,key,24,false,&out,16,NULL,0,false);close(db);
    api->log_value("cartridge_program_content_id",rc);if(rc)return rc;
    uint8_t id[16];__builtin_memcpy(id,out.data,16);
    rc=rawCall(api,ncm,4,&storage_id,1,false,&out,0,NULL,0,false);if(rc)return rc;
    if(out.move_count!=1)return 4;
    Handle storage=out.moves[0];char path[0x301]={0};
    rc=pointerCall(api,storage,8,id,16,path,0x300,true,&out);close(storage);
    api->log_value("cartridge_program_content_path",rc);if(rc)return rc;
    api->log(path);
    struct{uint8_t attr,pad[3];uint32_t type;uint64_t program;} in={1,{0},0,0};
    __builtin_memcpy(&in.program,ctx->input,8);
    rc=pointerCall(api,fs,10,&in,16,path,sizeof(path),false,&out);
    api->log_value("FS_open_native_cartridge_CodeFS",rc);
    if(!rc && out.move_count==1) {
        // Only retain the CodeFS, never the loader root.
        uint32_t* resource=(uint32_t*)(ctx->state+16448);
        if(*resource){close(out.moves[0]);return 5;}
        *resource=out.moves[0];api->log_value("retained_codefs",*resource);
    }
    return rc;
}
