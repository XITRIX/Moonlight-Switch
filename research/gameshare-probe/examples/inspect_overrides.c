#include <switch.h>
#include "research_api.h"
typedef struct {
    uint64_t open_offset,sm_offset; uint8_t open_code[16]; uint32_t transport,reserved;
    uint64_t override_offset; uint8_t override_code[16];
} Layout;
uint64_t probe_entry(const ResearchApi *api, ResearchContext *ctx) {
    if (ctx->input_size<sizeof(Layout)) return 1;
    Layout layout; __builtin_memcpy(&layout,ctx->input,sizeof(layout));
    uint64_t open=(uintptr_t)api->service_open;
    if (open<layout.open_offset) return 2;
    uint64_t base=open-layout.open_offset;
    uint8_t code[16]; Result rc=api->read_memory(base+layout.override_offset,code,16); if (rc) return rc;
    for (size_t i=0;i<16;i++) if (code[i]!=layout.override_code[i]) return 3;
    Handle (*get)(SmServiceName)=(void *)(uintptr_t)(base+layout.override_offset);
    // An automatic pointer array would need runtime relocations; call each constant directly.
#define CHECK(name) api->log_value(name,get(smEncodeName(name)))
    CHECK("fsp-ldr"); CHECK("fsp-srv"); CHECK("ldr:ro"); CHECK("ncm"); CHECK("lr"); CHECK("ns:am2");
#undef CHECK
    return 0;
}
