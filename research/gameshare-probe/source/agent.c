// SPDX-License-Identifier: MIT
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include "research_api.h"
#include "agent.h"
#include "agent_wire.h"

#define RESOURCE_COUNT 32
#define LOG_CAPACITY 16384
#define JSON_CAPACITY (RESEARCH_OUTPUT_SIZE*2+LOG_CAPACITY*6+8192)
typedef struct { uint32_t id; Service service; char name[9]; } Resource;
static Resource resources[RESOURCE_COUNT];
static uint32_t next_resource = 1;
static Handle raw_handles[64];
static Service private_sm;
static Thread job_thread;
static Mutex job_mutex;
static bool thread_live, busy, finished, poisoned;
static bool network_reset_requested;
static uint64_t job_id, began, ended, return_value;
static uint64_t revision;
static uint64_t session_id;
static Result job_result, close_result;
static char stage[40] = "idle", logs[LOG_CAPACITY];
static size_t log_length;
static uint32_t output_size;
static _Alignas(4096) uint8_t shared_state[RESEARCH_STATE_SIZE];
static _Alignas(4096) uint8_t output[RESEARCH_OUTPUT_SIZE];
static uint8_t *packet_bytes;
static AgentPacket packet_info;
static char json[JSON_CAPACITY];
static size_t json_size;
static const uint32_t invalid = MAKERESULT(Module_Libnx, LibnxError_BadInput);

static uint64_t milliseconds(void) { return armTicksToNs(armGetSystemTick())/1000000; }
static void setStage(const char *s) {
    mutexLock(&job_mutex); snprintf(stage, sizeof(stage), "%s", s); revision++; mutexUnlock(&job_mutex);
}
static void logText(const char *text) {
    if (!text) return;
    mutexLock(&job_mutex);
    size_t available = sizeof(logs)-log_length-1;
    // Native snippets are trusted code; cap how much the agent copies into a response.
    size_t length = strnlen(text, available);
    memcpy(logs+log_length, text, length); log_length += length;
    if (log_length < sizeof(logs)-1) logs[log_length++] = '\n';
    logs[log_length] = 0;
    revision++;
    mutexUnlock(&job_mutex);
}
static void logValue(const char *name, uint64_t value) {
    char message[192]; snprintf(message, sizeof(message), "%.120s = 0x%016" PRIx64, name?name:"value", value);
    logText(message);
}
static Resource *findResource(uint32_t id) {
    if (!id) return NULL;
    for (size_t i = 0; i < RESOURCE_COUNT; i++) if (resources[i].id == id) return &resources[i];
    return NULL;
}
static Resource *freeResource(void) {
    for (size_t i = 0; i < RESOURCE_COUNT; i++) if (!resources[i].id) return &resources[i];
    return NULL;
}
static uint32_t registerResource(Resource *slot, Service *service, const char *name) {
    if (!next_resource) return invalid;
    slot->id = next_resource++; slot->service = *service;
    snprintf(slot->name, sizeof(slot->name), "%.8s", name);
    return 0;
}
static Result initPrivateSm(void) {
    if (serviceIsActive(&private_sm)) return 0;
    Handle handle = INVALID_HANDLE;
    Result rc = svcConnectToNamedPort(&handle, "sm:");
    if (R_FAILED(rc)) return rc;
    serviceCreate(&private_sm, handle);
    uint64_t reserved = 0;
    rc = serviceDispatchIn(&private_sm, 0, reserved, .in_send_pid=true);
    if (R_FAILED(rc)) { serviceClose(&private_sm); memset(&private_sm, 0, sizeof(private_sm)); }
    return rc;
}
static uint32_t openService(const char *name, uint32_t *id) {
    if (!name || !id || !strnlen(name, 9) || strnlen(name, 9)>8) return invalid;
    *id = 0;
    Resource *slot = freeResource(); if (!slot || !next_resource) return invalid;
    Result rc = initPrivateSm(); if (R_FAILED(rc)) return rc;
    SmServiceName encoded = smEncodeName(name); Handle handle = INVALID_HANDLE;
    rc = serviceDispatchIn(&private_sm, 1, encoded,
        .out_handle_attrs={SfOutHandleAttr_HipcMove}, .out_handles=&handle);
    if (R_FAILED(rc)) return rc;
    Service service; serviceCreate(&service, handle);
    registerResource(slot, &service, name); *id = slot->id; return 0;
}
static uint32_t closeService(uint32_t id) {
    Resource *slot = findResource(id); if (!slot) return invalid;
    // Keep parents alive while domain children reference their session.
    if (slot->service.own_handle) {
        for (size_t i = 0; i < RESOURCE_COUNT; i++)
            if (resources[i].id && resources[i].id != id &&
                resources[i].service.session == slot->service.session) return invalid;
    }
    serviceClose(&slot->service); memset(slot, 0, sizeof(*slot)); return 0;
}
static uint32_t domainService(uint32_t id) {
    Resource *slot = findResource(id); return slot ? serviceConvertToDomain(&slot->service) : invalid;
}
static uint32_t closeRawHandle(uint32_t handle) {
    for (size_t i=0;i<64;i++) if (raw_handles[i]==handle && handle) {
        Result rc=svcCloseHandle(handle);
        if (R_SUCCEEDED(rc)) raw_handles[i]=0;
        return rc;
    }
    return invalid;
}
static uint32_t ipc(uint32_t id, const ResearchIpc *request, ResearchReply *reply) {
    Resource *slot = findResource(id);
    if (!slot || !request || !reply) return invalid;
    memset(reply, 0, sizeof(*reply)); reply->result = invalid;
    if (request->buffer_count>4 || request->input_handle_count>4 || request->input_object_count>4 ||
        request->output_object_count>4 || request->input_size>128 || request->output_size>128 ||
        (request->input_size && !request->input) || (request->output_size && !request->output)) return invalid;
    // Conservative upper bounds for the SDK's 0x100-byte TLS IPC buffer.
    if (64+request->input_size+14*request->buffer_count+4*request->input_handle_count+
        4*request->input_object_count > 256) return invalid;
    unsigned spare = 0; for (size_t i=0;i<RESOURCE_COUNT;i++) spare += !resources[i].id;
    if (spare < request->output_object_count || UINT32_MAX-next_resource < request->output_object_count) return invalid;
    unsigned handle_slots=0, handles_wanted=0;
    for (size_t i=0;i<64;i++) handle_slots+=!raw_handles[i];
    for (size_t i=0;i<4;i++) handles_wanted+=request->output_handle_attributes[i]!=0;
    if (handle_slots<handles_wanted) return invalid;
    Service objects[4] = {0}; Handle handles[8] = {0};
    SfDispatchParams params = {.in_send_pid=request->send_pid!=0,
        .in_num_handles=request->input_handle_count, .in_num_objects=request->input_object_count,
        .out_num_objects=request->output_object_count, .out_objects=objects, .out_handles=handles};
    uint32_t *attrs = &params.buffer_attrs.attr0;
    SfOutHandleAttr *handle_attrs = &params.out_handle_attrs.attr0;
    for (size_t i=0;i<4;i++) {
        if (request->output_handle_attributes[i]>2) return invalid;
        handle_attrs[i] = (SfOutHandleAttr)request->output_handle_attributes[i];
        if (i<request->buffer_count) {
            if (request->buffers[i].size>RESEARCH_INPUT_LIMIT ||
                (request->buffers[i].size && !request->buffers[i].data)) return invalid;
            attrs[i] = request->buffers[i].attributes;
            params.buffers[i] = (SfBuffer){request->buffers[i].data, request->buffers[i].size};
        }
        params.in_handles[i] = request->input_handles[i];
        if (i<request->input_object_count) {
            Resource *in = findResource(request->input_objects[i]); if (!in) return invalid;
            params.in_objects[i] = &in->service;
        }
    }
    Result rc = serviceDispatchImpl(&slot->service, request->command,
        request->input, request->input_size, request->output, request->output_size, params);
    reply->result = rc;
    if (R_SUCCEEDED(rc)) {
        for (uint32_t i=0;i<request->output_object_count;i++) {
            Resource *available = freeResource();
            registerResource(available, &objects[i], "object"); reply->objects[i] = available->id;
        }
        for (size_t i=0;i<4;i++) {
            reply->handles[i] = handles[i];
            if (handles[i]) for (size_t j=0;j<64;j++) if (!raw_handles[j]) { raw_handles[j]=handles[i]; break; }
        }
    }
    return rc;
}
static uint32_t queryMemory(uint64_t address, ResearchMemoryInfo *out) {
    if (!out) return invalid;
    MemoryInfo info; uint32_t page; Result rc = svcQueryMemory(&info, &page, address);
    if (R_SUCCEEDED(rc)) {
        *out = (ResearchMemoryInfo){.address=info.addr, .size=info.size, .type=info.type,
            .attributes=info.attr, .permissions=info.perm, .device_refcount=info.device_refcount,
            .ipc_refcount=info.ipc_refcount};
    }
    return rc;
}
static uint32_t readMemory(uint64_t at, void *out, uint32_t size) {
    if (!out || size>RESEARCH_OUTPUT_SIZE || UINT64_MAX-at<size) return invalid;
    uint8_t *p = out;
    while (size) {
        MemoryInfo info; uint32_t page; Result rc = svcQueryMemory(&info, &page, at);
        if (R_FAILED(rc)) return rc;
        if (!(info.perm & Perm_R) || (info.type&255)==MemType_Io ||
            at<info.addr || at-info.addr>=info.size) return invalid;
        uint64_t available=info.size-(at-info.addr); size_t n=available<size?(size_t)available:size;
        memcpy(p, (void *)(uintptr_t)at, n); at+=n; p+=n; size-=(uint32_t)n;
    }
    return 0;
}
static void sleepMs(uint32_t ms) { svcSleepThread((uint64_t)ms*1000000); }
static void requestNetworkReset(void) {
    mutexLock(&job_mutex); network_reset_requested=true; mutexUnlock(&job_mutex);
}
bool agentTakeNetworkReset(void) {
    mutexLock(&job_mutex);
    bool reset=network_reset_requested && !busy && !thread_live;
    if (reset) network_reset_requested=false;
    mutexUnlock(&job_mutex); return reset;
}
static uintptr_t resolve(const char *name) {
    if (!name) return 0;
    if (!strcmp(name,"probeReconnectNetwork")) return (uintptr_t)&requestNetworkReset;
#define SYMBOL(s) if (!strcmp(name, #s)) return (uintptr_t)&s
    SYMBOL(svcQueryMemory); SYMBOL(svcSendSyncRequest); SYMBOL(svcCloseHandle);
    SYMBOL(svcWaitSynchronization); SYMBOL(svcGetProcessId); SYMBOL(svcGetInfo);
    SYMBOL(aligned_alloc); SYMBOL(free); SYMBOL(svcCreateTransferMemory);
    SYMBOL(svcMapSharedMemory); SYMBOL(svcUnmapSharedMemory);
    SYMBOL(ldnInitialize); SYMBOL(ldnExit); SYMBOL(ldnSetProtocol); SYMBOL(ldnGetState);
    SYMBOL(ldnGetNetworkInfo); SYMBOL(appletGetAppletResourceUserId);
    SYMBOL(hidGetNpadStatesFullKey); SYMBOL(hidGetNpadStatesHandheld);
    SYMBOL(appletCreateLibraryApplet); SYMBOL(appletHolderStart); SYMBOL(appletHolderClose);
    SYMBOL(appletHolderRequestExit); SYMBOL(appletHolderCheckFinished); SYMBOL(appletHolderGetExitReason);
    SYMBOL(appletHolderPushInData); SYMBOL(appletHolderPopOutData);
    SYMBOL(appletCreateStorage); SYMBOL(appletStorageWrite); SYMBOL(appletStorageRead);
    SYMBOL(appletStorageGetSize); SYMBOL(appletStorageClose);
    SYMBOL(libappletArgsCreate); SYMBOL(libappletArgsPush);
#undef SYMBOL
    return 0;
}
static const ResearchApi api = {.abi=RESEARCH_ABI, .struct_size=sizeof(ResearchApi),
    .log=logText, .log_value=logValue, .service_open=openService, .service_close=closeService,
    .service_domain=domainService, .ipc=ipc, .query_memory=queryMemory, .read_memory=readMemory,
    .sleep_ms=sleepMs, .time_ms=milliseconds, .resolve=resolve, .handle_close=closeRawHandle};

static void executeJob(void *unused) {
    (void)unused;
    Jit code = {0}; bool created = false;
    setStage("jit_create");
    Result rc = jitCreate(&code, (packet_info.code_size+0xfff)&~(size_t)0xfff);
    if (R_SUCCEEDED(rc)) {
        created = true; setStage("jit_writable"); rc = jitTransitionToWritable(&code);
    }
    if (R_SUCCEEDED(rc)) {
        memset(jitGetRwAddr(&code), 0, code.size);
        memcpy(jitGetRwAddr(&code), packet_bytes+AGENT_HEADER_SIZE, packet_info.code_size);
        setStage("jit_publish"); rc = jitTransitionToExecutable(&code);
    }
    if (R_SUCCEEDED(rc)) {
        setStage("native_entry");
        armICacheInvalidate(jitGetRxAddr(&code), code.size);
        ResearchContext context = {.abi=RESEARCH_ABI, .struct_size=sizeof(ResearchContext),
            .input=packet_bytes+AGENT_HEADER_SIZE+packet_info.code_size, .input_size=packet_info.input_size,
            .output=output, .output_capacity=sizeof(output), .state=shared_state, .state_size=sizeof(shared_state)};
        ResearchEntry entry = (ResearchEntry)((uint8_t *)jitGetRxAddr(&code)+packet_info.entry);
        uint64_t returned = entry(&api, &context);
        mutexLock(&job_mutex); return_value=returned;
        if (context.output_size<=sizeof(output)) output_size=context.output_size;
        else rc=invalid;
        mutexUnlock(&job_mutex);
    }
    Result cleanup = 0;
    if (created) { setStage("jit_close"); cleanup=jitClose(&code); }
    mutexLock(&job_mutex);
    job_result=rc; close_result=cleanup; poisoned=R_FAILED(cleanup);
    ended=milliseconds(); strcpy(stage, R_SUCCEEDED(rc) && !poisoned ? "completed" : "failed");
    finished=true;
    revision++;
    mutexUnlock(&job_mutex);
}
void agentPoll(void) {
    mutexLock(&job_mutex); bool done=thread_live && finished; mutexUnlock(&job_mutex);
    if (!done) return;
    threadWaitForExit(&job_thread); threadClose(&job_thread);
    free(packet_bytes); packet_bytes=NULL;
    mutexLock(&job_mutex); thread_live=false; busy=false; revision++; mutexUnlock(&job_mutex);
}
bool agentBusy(void) { mutexLock(&job_mutex); bool value=busy; mutexUnlock(&job_mutex); return value; }
uint64_t agentRevision(void) { mutexLock(&job_mutex); uint64_t value=revision; mutexUnlock(&job_mutex); return value; }
void agentSummary(char *text, size_t capacity) {
    mutexLock(&job_mutex);
    snprintf(text,capacity,"Job %" PRIu64 ": %s | result %08x | return 0x%" PRIx64,
             job_id,stage,job_result,return_value);
    mutexUnlock(&job_mutex);
}
void agentLogTail(char *text, size_t capacity) {
    if (!capacity) return;
    mutexLock(&job_mutex);
    size_t start=log_length; unsigned lines=0;
    while (start && lines<7) { --start; if (logs[start]=='\n') ++lines; }
    if (lines==7) ++start;
    if (log_length-start>=capacity) start=log_length-(capacity-1);
    size_t length=log_length-start;
    memcpy(text,logs+start,length); text[length]=0;
    mutexUnlock(&job_mutex);
}
int agentSubmit(const void *data, size_t size) {
    agentPoll();
    if (agentBusy() || poisoned) return 409;
    AgentPacket parsed;
    if (agentParsePacket(data,size,&parsed)) return 400;
    packet_bytes=malloc(size); if (!packet_bytes) return 503;
    memcpy(packet_bytes,data,size); packet_info=parsed;
    mutexLock(&job_mutex);
    busy=true; finished=false; began=milliseconds(); ended=0; return_value=0; output_size=0;
    job_result=close_result=0; logs[0]=0; log_length=0; memset(output,0,sizeof(output));
    job_id++; revision++; strcpy(stage,"thread_create");
    mutexUnlock(&job_mutex);
    Result rc=threadCreate(&job_thread,executeJob,NULL,NULL,0x20000,0x2c,-2);
    if (R_SUCCEEDED(rc)) {
        rc=threadStart(&job_thread);
        if (R_FAILED(rc)) threadClose(&job_thread);
    }
    if (R_FAILED(rc)) {
        free(packet_bytes); packet_bytes=NULL;
        mutexLock(&job_mutex); busy=false; finished=true; job_result=rc;
        strcpy(stage,"thread_failed"); ended=milliseconds(); revision++; mutexUnlock(&job_mutex);
        return 503;
    }
    thread_live=true; return 202;
}
static void append(const char *format, ...) {
    if (json_size>=sizeof(json)-1) return;
    va_list args; va_start(args,format);
    int n=vsnprintf(json+json_size,sizeof(json)-json_size,format,args); va_end(args);
    if (n<0 || (size_t)n>=sizeof(json)-json_size) { json_size=sizeof(json)-1; return; }
    json_size+=(size_t)n;
}
static void quote(const char *s) {
    append("\"");
    for (;*s;s++) {
        unsigned char c=*s;
        if (c=='"' || c=='\\') append("\\%c",c);
        else if (c<32 || c>=127) append("\\u%04x",c);
        else append("%c",c);
    }
    append("\"");
}
const char *agentStatus(void) {
    agentPoll(); json_size=0;
    if (!session_id) {
        randomGet(&session_id,sizeof(session_id));
        // Some homebrew RNG implementations repeat their initial output on reload.
        session_id^=armGetSystemTick();
        if (!session_id) session_id=1;
    }
    mutexLock(&job_mutex);
    append("{\"schema\":\"kaihen-research-agent-v1\",\"api_abi\":1,\"session_id\":\"%016" PRIx64 "\",\"job_id\":%" PRIu64
           ",\"busy\":%s,\"finished\":%s,\"poisoned\":%s,\"stage\":",
           session_id,job_id,busy?"true":"false",finished?"true":"false",poisoned?"true":"false");
    quote(stage);
    append(",\"elapsed_ms\":%" PRIu64 ",\"result\":\"%08x\",\"jit_close_result\":\"%08x\","
           "\"return_value\":\"0x%016" PRIx64 "\",\"code_size\":%u,\"entry_offset\":%u,\"log\":",
           began?(ended?ended:milliseconds())-began:0,job_result,close_result,return_value,packet_info.code_size,packet_info.entry);
    quote(logs);
    append(",\"output_hex\":\"");
    // The output buffer belongs to the native worker until it has finished.
    if (finished) for (size_t i=0;i<output_size;i++) append("%02x",output[i]);
    append("\",\"log_truncated\":%s,\"resources\":[",log_length>=sizeof(logs)-2?"true":"false");
    // Services are mutated only by a job; enumerate them only after it stops.
    if (!busy) {
        bool first=true;
        for (size_t i=0;i<RESOURCE_COUNT;i++) if (resources[i].id) {
            if (!first) append(",");
            first=false;
            append("{\"id\":%u,\"name\":",resources[i].id); quote(resources[i].name);
            append(",\"domain\":%s,\"object_id\":%u}",
                   serviceIsDomain(&resources[i].service)||serviceIsDomainSubservice(&resources[i].service)?"true":"false",
                   resources[i].service.object_id);
        }
    }
    append("],\"raw_handles\":[");
    if (!busy) {
        bool first=true;
        for (size_t i=0;i<64;i++) if (raw_handles[i]) {
            if (!first) append(",");
            append("%u",raw_handles[i]); first=false;
        }
    }
    append("],\"resource_snapshot_available\":%s}\n",busy?"false":"true");
    mutexUnlock(&job_mutex);
    if (json_size>=sizeof(json)-1) return "{\"error\":\"agent_status_overflow\"}\n";
    return json;
}
void agentShutdown(void) {
    agentPoll();
    if (agentBusy()) return;
    for (size_t i=0;i<64;i++) if (raw_handles[i]) { svcCloseHandle(raw_handles[i]); raw_handles[i]=0; }
    // Domain children first, then roots; close remaining kernel sessions normally.
    for (size_t pass=0;pass<2;pass++) for (size_t i=0;i<RESOURCE_COUNT;i++) {
        Resource *r=&resources[i];
        if (r->id && (pass || !r->service.own_handle)) { serviceClose(&r->service); memset(r,0,sizeof(*r)); }
    }
    if (serviceIsActive(&private_sm)) serviceClose(&private_sm);
}
