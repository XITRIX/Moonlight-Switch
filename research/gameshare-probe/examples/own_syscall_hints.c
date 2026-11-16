// Inspect this NRO's loader-supplied hints only. Does not execute any debug SVC.
// Exact installed 0.3.1 ELF/NRO guards; regenerate before using another binary.
#include "own_vi2_symbols.h"
uint64_t probe_entry(const ResearchApi *api,ResearchContext *ctx) {
    uintptr_t anchor=own_vi2_symbol(api,0);if(!anchor || ctx->output_capacity<24)return 1;
    uintptr_t base=anchor-vi2_own_symbols[0].offset;
    // Include ADRP+ADD+LDR, so both the BSS page and low address bits match.
    const uint8_t expected[32]={0x1f,0xfc,0x02,0x71,0x08,0x01,0x00,0x54,
        0xe1,0x0b,0x00,0xf0,0x02,0x7c,0x06,0x53,
        0x21,0x40,0x23,0x91,0x21,0x78,0x62,0xf8,
        0x20,0x24,0xc0,0x9a,0x00,0x00,0x00,0x12};
    uint8_t actual[32];uint32_t rc=api->read_memory(base+0x22280,actual,sizeof(actual));
    if(rc)return rc;
    for(unsigned i=0;i<sizeof(actual);i++)if(actual[i]!=expected[i])return 2;
    uint64_t masks[3];rc=api->read_memory(base+0x1a18d0,masks,sizeof(masks));
    if(rc)return rc;
    for(unsigned i=0;i<3;i++)api->log_value("own_loader_syscall_mask",masks[i]);
    const unsigned calls[]={0x60,0x69,0x6a,0x76,0x77};
    for(unsigned i=0;i<sizeof(calls)/sizeof(calls[0]);i++){
        api->log_value("syscall_number",calls[i]);
        api->log_value("loader_hint",(masks[calls[i]/64]>>(calls[i]%64))&1);
    }
    api->log("Hints are loader metadata, not kernel permission; no debug SVC executed");
    __builtin_memcpy(ctx->output,masks,24);ctx->output_size=24;return 0;
}
