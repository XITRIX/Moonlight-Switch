// One autonomous preparation cycle. Never opens an applet or writes game saves.
// Uses exact 23.0.1 command sizes and source-derived semantics; reports failures.
#include <switch.h>
#include "research_api.h"
#include "streamplay_guest_display.h"
typedef struct { uint8_t type,mode; uint16_t zero; uint32_t guests; } MemoryConfig;
typedef struct { MemoryConfig config; uint64_t pid; } InitializeConfig;
typedef struct { uint32_t guest,pad; uint64_t sizes[3]; } GuestBuffers;
typedef struct {
    uint8_t min_guests,max_guests,min_players,max_players;
    uint32_t pad; uint32_t flags; uint16_t feature,other; uint8_t reserved[16];
} MatchConfig;
typedef struct { uint64_t magic; void *memory; uint64_t bytes; uint32_t handles[4]; } WorkMemory;
_Static_assert(sizeof(MemoryConfig)==8 && sizeof(InitializeConfig)==16,"initialize ABI");
_Static_assert(sizeof(GuestBuffers)==32 && sizeof(MatchConfig)==32,"prepare ABI");
typedef void *(*Allocate)(size_t,size_t);
typedef void (*Release)(void *);
typedef Result (*CreateTransfer)(Handle *,void *,uint64_t,uint32_t);
typedef Result (*CloseNative)(Handle);
static uint32_t call(const ResearchApi *api,uint32_t scoped,uint32_t cmd,
    const void *in,uint32_t in_size,void *out,uint32_t out_size,
    const uint32_t *handles,uint32_t count,uint32_t pid) {
    ResearchIpc req={.command=cmd,.input=in,.input_size=in_size,.output=out,.output_size=out_size,
        .input_handle_count=count,.send_pid=pid};
    for (uint32_t i=0;i<count;i++) req.input_handles[i]=handles[i];
    ResearchReply reply={0};
    uint32_t rc=api->ipc(scoped,&req,&reply);
    api->log_value("command",cmd); api->log_value("result",rc);
    return rc;
}
uint64_t probe_entry(const ResearchApi *api,ResearchContext *ctx) {
    if (ctx->state_size<800 || ctx->output_capacity<128) return 1;
    uint32_t scoped=*(uint32_t *)(ctx->state+12);
    uint32_t *initialized=(uint32_t *)(ctx->state+128);
    WorkMemory *work=(WorkMemory *)(ctx->state+256);
    uint32_t pctl_root=0,pctl=0,connection_event=0; bool communication_started=false;
    GuestDisplay *display=(GuestDisplay *)(ctx->state+512);
    bool use_display=false;uint32_t tail_kind=0;
    uint32_t match_flags=8;uint16_t match_feature=0,match_other=0;uint8_t frame_rate=0;
    if (!scoped || *initialized || work->memory) return 2;
    Allocate allocate=(Allocate)api->resolve("aligned_alloc");
    Release release=(Release)api->resolve("free");
    CreateTransfer create=(CreateTransfer)api->resolve("svcCreateTransferMemory");
    CloseNative close=(CloseNative)api->resolve("svcCloseHandle");
    void (*reconnect)(void)=(void (*)(void))api->resolve("probeReconnectNetwork");
    if (!allocate || !release || !create || !close || !reconnect) return 3;
    MemoryConfig cfg={.guests=1};
    uint32_t match_command=504;
    if (ctx->input_size==16) {
        // Explicit experiment input: type, mode, newer-command, display, zero,
        // frame-rate, zero[2], u32 flags, u16 feature, u16 other. Keep the
        // native parental communication check mandatory for every variation.
        if(ctx->input[0]>1 || ctx->input[1]>1 || ctx->input[2]>1 ||
           ctx->input[3]>1 || ctx->input[4] || ctx->input[5]>2 ||
           ctx->input[6] || ctx->input[7])return 4;
        cfg.type=ctx->input[0];cfg.mode=ctx->input[1];
        match_command=ctx->input[2]?511:504;use_display=ctx->input[3]!=0;
        frame_rate=ctx->input[5];
        __builtin_memcpy(&match_flags,ctx->input+8,4);
        __builtin_memcpy(&match_feature,ctx->input+12,2);
        __builtin_memcpy(&match_other,ctx->input+14,2);
        if(match_flags>=64 || (match_flags&24)==24 || match_feature>=32 ||
           (match_other!=0 && match_other!=1 && match_other!=3))return 4;
    } else if (ctx->input_size) {
        if ((ctx->input_size<2 || ctx->input_size>5) || ctx->input[0]>1 || ctx->input[1]>1) return 4;
        cfg.type=ctx->input[0];cfg.mode=ctx->input[1];
        if (ctx->input_size>=3) { if(ctx->input[2]!=1)return 4;match_command=511; }
        if (ctx->input_size>=4) { if(ctx->input[3]!=1)return 4;use_display=true; }
        if (ctx->input_size==5) {
            tail_kind=ctx->input[4];if(tail_kind<1 || tail_kind>3 || (tail_kind==1&&!ctx->state[784]))return 4;
        }
    }
    uint32_t required=0;
    uint32_t rc=call(api,scoped,1,&cfg,8,&required,4,0,0,0);if(rc)return rc;
    uint64_t sizes[4]={0}; MemoryConfig query=cfg;query.guests=0;
    rc=call(api,scoped,9,&query,8,sizes,32,0,0,0);if(rc)return rc;
    uint64_t total=0;
    for (uint32_t i=0;i<4;i++) {
        api->log_value("work_size",sizes[i]);
        if (sizes[i]>128*1024*1024 || (sizes[i]&4095)) return 5;
        total+=sizes[i];
    }
    if (!total || total!=required || total>128*1024*1024) return 6;
    work->memory=allocate(4096,(size_t)total);
    if (!work->memory) return 7;
    work->magic=0x5354504c574f524bULL;work->bytes=total;
    __builtin_memset(work->handles,0,sizeof(work->handles));
    __builtin_memset(work->memory,0,(size_t)total);
    if(use_display) { rc=guest_display_open(api,display);if(rc)goto cleanup; }
    InitializeConfig init={.config=cfg};
    rc=call(api,scoped,2,&init,16,0,0,0,0,1);
    if(rc)goto cleanup;
    *initialized=1;
    // The recovered SDK obtains/retains this event during Initialize, before
    // audio setup. Duplicate only our handle; release it after finalization.
    ResearchReply event_reply={0};
    ResearchIpc event_req={.command=7,.output_handle_attributes={1}};
    rc=api->ipc(scoped,&event_req,&event_reply);
    api->log_value("connection_event_result",rc);if(rc)goto cleanup;
    connection_event=event_reply.handles[0];
    if(!connection_event){rc=13;goto cleanup;}
    for(uint32_t cmd=502;cmd<=503;cmd++) {
        uint8_t echoed=255;uint32_t query_rc=call(api,scoped,cmd,0,0,&echoed,1,0,0,0);
        if(!query_rc)api->log_value("initialized_config_byte",echoed);
        if(query_rc){rc=query_rc;goto cleanup;}
        if(echoed!=(cmd==502?cfg.type:cfg.mode)){
            api->log("Initialized config differs from requested config; stopping before matchmaking");
            rc=14;goto cleanup;
        }
    }
    // Initialize also supplies current audio-output settings before matchmaking.
    struct { uint32_t auto_tune; float speaker,tv,external; } volumes={0,1.0f,1.0f,1.0f};
    rc=call(api,scoped,510,&volumes,16,0,0,0,0,0);if(rc)goto cleanup;
    // Follow the SDK's native communication permission check. Do not change
    // restrictions or continue if the system denies communication.
    const char *pctl_names[]={"pctl:a","pctl:s","pctl:r","pctl"};
    for(uint32_t i=0;i<4;i++) {
        api->log(pctl_names[i]);rc=api->service_open(pctl_names[i],&pctl_root);
        api->log_value("pctl_open_result",rc);if(!rc)break;
    }
    if(rc)goto cleanup;
    rc=api->service_domain(pctl_root);if(rc)goto cleanup;
    uint64_t pid_placeholder=0;ResearchReply pctl_reply={0};
    ResearchIpc pctl_req={.command=0,.input=&pid_placeholder,.input_size=8,.send_pid=1,.output_object_count=1};
    rc=api->ipc(pctl_root,&pctl_req,&pctl_reply);
    api->log_value("pctl_create_result",rc);if(rc)goto cleanup;
    pctl=pctl_reply.objects[0];
    rc=call(api,pctl,1021,0,0,0,0,0,0,0);if(rc)goto cleanup;
    communication_started=true;
    // Reconstructed SDK places first, then third, then second in guest memory,
    // and supplies copy handles and sizes in that same order (including zero).
    uint64_t ordered[]={sizes[0],sizes[1],sizes[3],sizes[2]};
    uint64_t offset=0;
    for(uint32_t i=0;i<4;i++) {
        if(ordered[i]) {
            rc=create(&work->handles[i],(uint8_t *)work->memory+offset,ordered[i],Perm_None);
            api->log_value("transfer_index",i);api->log_value("create_transfer_result",rc);
            if(rc)goto cleanup;
        }
        offset+=ordered[i];
    }
    rc=call(api,scoped,505,&ordered[0],8,0,0,&work->handles[0],1,0);if(rc)goto cleanup;
    GuestBuffers guest={.sizes={ordered[1],ordered[2],ordered[3]}};
    rc=call(api,scoped,506,&guest,32,0,0,&work->handles[1],3,0);if(rc)goto cleanup;
    uint32_t style=7;
    rc=call(api,scoped,25,&style,4,0,0,0,0,0);if(rc)goto cleanup;
    MatchConfig match={.min_guests=1,.max_guests=1,.min_players=1,.max_players=1,
        .flags=match_flags,.feature=match_feature,.other=match_other};
    api->log_value("match_flags",match_flags);
    api->log_value("match_feature",match_feature);
    api->log_value("match_other",match_other);
    api->log_value("frame_rate",frame_rate);
    // Explicit layout hypothesis: the opaque final 16 bytes may be account::Uid.
    // Only use the actual local selected user; never log its identifier.
    if(tail_kind==1)__builtin_memcpy(match.reserved,ctx->state+768,16);
    if(tail_kind>=2) {
        Result (*get_info)(u64*,u32,Handle,u64)=(void*)api->resolve("svcGetInfo");
        uint64_t own_program=0;if(!get_info){rc=12;goto cleanup;}
        rc=get_info(&own_program,InfoType_ProgramId,CUR_PROCESS_HANDLE,0);if(rc)goto cleanup;
        // Layout hypothesis: a declared local-communication ID in the opaque tail.
        // ARP read confirmed this application's allowed IDs equal its own ProgramId.
        __builtin_memcpy(match.reserved+(tail_kind==2?0:8),&own_program,8);
    }
    struct { MatchConfig match; uint8_t frame_config[32]; } match_new={.match=match};
    match_new.frame_config[0]=frame_rate;
    rc=call(api,scoped,match_command,&match_new,match_command==511?64:32,ctx->output,128,0,0,0);
    if(!rc) { ctx->output_size=128;api->log("Matchmaking arguments obtained; no applet or advertisement started"); }
cleanup:
    if(communication_started) {
        uint32_t done=call(api,pctl,1022,0,0,0,0,0,0,0);
        if(done && !rc)rc=done;
    }
    if(pctl)api->log_value("pctl_close_result",api->service_close(pctl));
    if(pctl_root)api->log_value("pctl_root_close_result",api->service_close(pctl_root));
    if(*initialized) {
        uint32_t done=call(api,scoped,3,0,0,0,0,0,0,0);
        if(done) { api->log("Finalize failed: retaining buffer and owned handles; do not free while shared");return rc?rc:done; }
        *initialized=0;reconnect();
    }
    if(connection_event) {
        uint32_t done=api->handle_close(connection_event);
        api->log_value("close_connection_event",done);if(done&&!rc)rc=done;
    }
    if(use_display) { uint32_t done=guest_display_close(api,display);if(done && !rc)rc=done; }
    uint32_t close_error=0;
    for(uint32_t i=0;i<4;i++)if(work->handles[i]) {
        uint32_t done=close(work->handles[i]);
        api->log_value("close_transfer_result",done);
        if(!done)work->handles[i]=0;else close_error=done;
    }
    if(close_error) { api->log("Retaining allocation after failed handle close");return rc?rc:close_error; }
    release(work->memory);__builtin_memset(work,0,sizeof(*work));
    return rc;
}
