// SPDX-License-Identifier: MIT
// Read only the installed HEN binary, through qualified functions in this NRO.
#include "research_api.h"
typedef struct {
    uint64_t open_offset; uint8_t open_code[16];
    uint64_t offsets[3]; uint8_t expected[3][16];
} Layout;
_Static_assert(sizeof(Layout)==96,"layout ABI");
typedef void* (*Open)(const char*,const char*);
typedef size_t (*Read)(void*,size_t,size_t,void*);
typedef int (*Close)(void*);
uint64_t probe_entry(const ResearchApi* api,ResearchContext* ctx) {
    if(ctx->input_size!=sizeof(Layout))return 1;
    Layout in;__builtin_memcpy(&in,ctx->input,sizeof(in));
    uint64_t start=(uintptr_t)api->service_open;
    if(start<in.open_offset)return 2;
    uint64_t base=start-in.open_offset;uint8_t bytes[16];
    if(api->read_memory(start,bytes,16))return 3;
    for(unsigned i=0;i<16;i++)if(bytes[i]!=in.open_code[i])return 4;
    for(unsigned f=0;f<3;f++){
        if(base+in.offsets[f]<base || api->read_memory(base+in.offsets[f],bytes,16))return 5;
        for(unsigned i=0;i<16;i++)if(bytes[i]!=in.expected[f][i])return 6;
    }
    Open open=(Open)(uintptr_t)(base+in.offsets[0]);
    Read read=(Read)(uintptr_t)(base+in.offsets[1]);
    Close close=(Close)(uintptr_t)(base+in.offsets[2]);
    void** state=(void**)(ctx->state+16512);
    if(!*state){*state=open("sdmc:/ouncehen/bin/hen/main","rb");if(!*state)return 7;}
    size_t got=read(ctx->output,1,65536,*state);ctx->output_size=(uint32_t)got;
    if(got<65536){int rc=close(*state);*state=0;if(rc)return 8;}
    return 0;
}
