// SPDX-License-Identifier: MIT
#include <switch.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "module_scan.h"
#include "agent.h"
#include "agent_wire.h"
#include "research_api.h"

#define PORT 39381
#define VERSION "0.3.1"
#define RUN_BASELINE 0
#define REPORT_CAPACITY (512*1024)
static const char *service_names[] = {
    "stpl:u", "lrp:u", "stpl:sys", "chat:u", "chat:sp", "ldn:u", "ldn:s", "lp2p:app", "spl:lrp"
};
#define SERVICE_COUNT (sizeof(service_names)/sizeof(service_names[0]))
typedef struct { int state; Result result; uint64_t elapsed_ms; char phase[32]; } ServiceResult;
static ServiceResult service_results[SERVICE_COUNT];
static ServiceResult streamplay_creator;
static bool streamplay_object;
static Mutex result_mutex;
static bool worker_done;
static unsigned generation;
static Thread worker;
static ProbeInventory inventory;
static MemoryInfo regions[PROBE_MAX_REGIONS];
static size_t region_count;
static char report[REPORT_CAPACITY], access_key[17];
static size_t report_length;
static bool report_overflow;
static uint64_t started;
static Result network_result, worker_result;
static bool server_listening;
static int server_errno;
static char firmware[32] = "unknown", ip_text[INET_ADDRSTRLEN] = "unknown";
static Result firmware_result;
static bool sd_mounted, sd_owned;
static int persistence_errno;
static uint64_t nowMs(void) { return armTicksToNs(armGetSystemTick())/1000000; }

static bool codeType(uint32_t type) {
    type &= 0xff;
    return type == MemType_CodeStatic || type == MemType_CodeMutable ||
        type == MemType_ModuleCodeStatic || type == MemType_ModuleCodeMutable;
}
static bool readMemory(uint64_t at, void *out, size_t size, void *unused) {
    (void)unused;
    if (UINT64_MAX-at < size) return false;
    uint8_t *dest = out;
    while (size) {
        size_t lo = 0, hi = region_count;
        while (lo < hi) {
            size_t mid = lo+(hi-lo)/2;
            if (regions[mid].addr <= at) lo = mid+1; else hi = mid;
        }
        if (!lo) return false;
        MemoryInfo *r = &regions[lo-1];
        if (!codeType(r->type) || !(r->perm & Perm_R) || at-r->addr >= r->size) return false;
        uint64_t available = r->size-(at-r->addr);
        size_t n = available < size ? (size_t)available : size;
        memcpy(dest, (void *)(uintptr_t)at, n);
        dest += n; at += n; size -= n;
    }
    return true;
}
static bool executableAt(uint64_t address) {
    for (size_t i = 0; i < region_count; i++) {
        MemoryInfo *r = &regions[i];
        if (address >= r->addr && address-r->addr < r->size)
            return codeType(r->type) && (r->perm & Perm_X);
    }
    return false;
}
static void scanModules(void) {
    // Query the whole own address space; never request another process handle.
    uint64_t address = 0;
    for (size_t i = 0; i < PROBE_MAX_REGIONS; i++) {
        MemoryInfo r; uint32_t page;
        Result rc = svcQueryMemory(&r, &page, address);
        inventory.query_result = rc;
        if (R_FAILED(rc)) break;
        regions[region_count++] = r;
        inventory.regions_visited++;
        if (!r.size || UINT64_MAX-r.addr < r.size) { inventory.regions_complete = true; break; }
        uint64_t end = r.addr+r.size;
        if (end <= address) break;
        address = end;
        // Switch 2 user address spaces are at most 39 bits in this SDK profile.
        if (address >= (UINT64_C(1)<<39)) { inventory.regions_complete = true; break; }
    }
    for (size_t i = 0; i < region_count; i++) {
        MemoryInfo *r = &regions[i];
        if (codeType(r->type) && (r->perm & (Perm_R|Perm_X)) == (Perm_R|Perm_X))
            probeScanModule(&inventory, r->addr, readMemory, NULL);
    }
    for (size_t i = 0; i < inventory.symbol_count; i++) {
        ProbeSymbol *s = &inventory.symbols[i];
        s->executable = s->executable && executableAt(s->address);
    }
}
static void publish(size_t i, int state, Result rc, uint64_t elapsed, const char *phase) {
    mutexLock(&result_mutex);
    service_results[i].state = state; service_results[i].result = rc;
    service_results[i].elapsed_ms = elapsed;
    snprintf(service_results[i].phase, sizeof(service_results[i].phase), "%s", phase);
    generation++;
    mutexUnlock(&result_mutex);
}
static void probeStreamplayCreator(Handle handle) {
    uint64_t before = nowMs();
    mutexLock(&result_mutex);
    streamplay_creator.state = 1;
    streamplay_creator.elapsed_ms = before-started;
    strcpy(streamplay_creator.phase, "documented_no_input_cmd0");
    generation++;
    mutexUnlock(&result_mutex);
    Service root = {0}, shim = {0};
    serviceCreate(&root, handle);
    // Switch 2 public ABI: stpl:u command 0, no input, one returned object.
    // This creates a scoped interface only; no commands are sent to that object.
    Result rc = serviceDispatch(&root, 0, .out_num_objects=1, .out_objects=&shim);
    bool obtained = R_SUCCEEDED(rc) && serviceIsActive(&shim);
    if (serviceIsActive(&shim)) serviceClose(&shim);
    serviceClose(&root);
    mutexLock(&result_mutex);
    streamplay_creator.state = obtained ? 2 : 3;
    streamplay_creator.result = rc;
    streamplay_creator.elapsed_ms = nowMs()-before;
    strcpy(streamplay_creator.phase, "scoped_object_closed");
    streamplay_object = obtained;
    generation++;
    mutexUnlock(&result_mutex);
}
static void serviceWorker(void *unused) {
    (void)unused;
    // A private SM session avoids sharing the runtime's SM/TLS transaction state.
    // Only the documented stpl:u scoped-interface creator is called below.
    Service sm = {0}; Handle handle = INVALID_HANDLE;
    publish(0, 1, 0, 0, "connect_sm");
    Result rc = svcConnectToNamedPort(&handle, "sm:");
    if (R_SUCCEEDED(rc)) {
        serviceCreate(&sm, handle);
        uint64_t reserved = 0;
        publish(0, 1, 0, 0, "register_sm_client");
        rc = serviceDispatchIn(&sm, 0, reserved, .in_send_pid = true);
    }
    if (R_SUCCEEDED(rc)) {
        for (size_t i = 0; i < SERVICE_COUNT; i++) {
            uint64_t before = nowMs();
            publish(i, 1, 0, before-started, "get_service_handle");
            SmServiceName name = smEncodeName(service_names[i]);
            Handle opened = INVALID_HANDLE;
            rc = serviceDispatchIn(&sm, 1, name,
                .out_handle_attrs = {SfOutHandleAttr_HipcMove}, .out_handles = &opened);
            bool obtained = R_SUCCEEDED(rc) && opened != INVALID_HANDLE;
            publish(i, obtained ? 2 : 3, rc, nowMs()-before, obtained ? "handle_obtained" : "open_failed");
            if (obtained) {
                if (i == 0) probeStreamplayCreator(opened);
                else svcCloseHandle(opened);
                publish(i, 2, rc, nowMs()-before, "closed_after_open_attempt");
            }
        }
    } else {
        publish(0, 3, rc, nowMs()-started, "sm_setup_failed");
    }
    if (serviceIsActive(&sm)) serviceClose(&sm);
    mutexLock(&result_mutex); worker_done = true; generation++; mutexUnlock(&result_mutex);
}

static void append(const char *format, ...) {
    if (report_overflow) return;
    va_list args; va_start(args, format);
    int n = vsnprintf(report+report_length, REPORT_CAPACITY-report_length, format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= REPORT_CAPACITY-report_length) { report_overflow = true; return; }
    report_length += (size_t)n;
}
static void quoted(const char *s) {
    append("\"");
    for (; *s; s++) {
        unsigned char c = *s;
        if (c == '"' || c == '\\') append("\\%c", c);
        else if (c < 32 || c >= 127) append("\\u%04x", c);
        else append("%c", c);
    }
    append("\"");
}
static void resultJson(Result rc) {
    append("{\"hex\":\"%08x\",\"module\":%u,\"description\":%u}", rc, R_MODULE(rc), R_DESCRIPTION(rc));
}
static unsigned makeReport(void) {
    ServiceResult snapshot[SERVICE_COUNT], creator; bool done, object; unsigned gen;
    mutexLock(&result_mutex);
    memcpy(snapshot, service_results, sizeof(snapshot)); done = worker_done; gen = generation;
    creator = streamplay_creator; object = streamplay_object;
    mutexUnlock(&result_mutex);
    report_length = 0; report_overflow = false;
    append("{\"schema\":\"gameshare-probe-v1\",\"version\":\"" VERSION "\",\"remote_execution_enabled\":true,\"evidence\":\"console_runtime\","
           "\"uptime_ms\":%" PRIu64 ",\"generation\":%u,\"firmware\":", nowMs()-started, gen);
    quoted(firmware); append(",\"firmware_result\":"); resultJson(firmware_result);
    append(",\"hosversion_hint\":%u,\"kaihen_hint\":%s,\"loader\":", hosversionGet(), hosversionIsKaiHEN()?"true":"false");
    char loader[257] = {0}; const char *info = envGetLoaderInfo();
    size_t len = envGetLoaderInfoSize(); if (len > 256) len = 256;
    if (info && len) memcpy(loader, info, len);
    quoted(loader); append(",\"network_result\":"); resultJson(network_result);
    append(",\"server_listening\":%s,\"server_errno\":%d,\"ip\":", server_listening?"true":"false", server_errno);
    quoted(ip_text);
    append(",\"worker_result\":"); resultJson(worker_result);
    append(",\"sd_mounted\":%s,\"last_persistence_errno\":%d,\"worker_done\":%s,\"services\":[",
           sd_mounted?"true":"false", persistence_errno, done?"true":"false");
    const char *states[] = {"not_attempted", "pending", "opened", "error"};
    for (size_t i = 0; i < SERVICE_COUNT; i++) {
        ServiceResult *s = &snapshot[i];
        if (i) append(",");
        append("{\"name\":"); quoted(service_names[i]); append(",\"state\":"); quoted(states[s->state]);
        append(",\"phase\":"); quoted(s->phase);
        append(",\"elapsed_ms\":%" PRIu64 ",\"result\":", s->state == 1 ? nowMs()-started-s->elapsed_ms : s->elapsed_ms);
        if (s->state == 2 || s->state == 3) resultJson(s->result); else append("null");
        append("}");
    }
    append("],\"streamplay_creator\":{\"service\":\"stpl:u\",\"protocol\":\"CMIF\",\"command\":0,"
           "\"input_bytes\":0,\"state\":");
    quoted(states[creator.state]);
    append(",\"phase\":"); quoted(creator.phase);
    append(",\"object_obtained\":%s,\"object_commands_sent\":0,\"elapsed_ms\":%" PRIu64 ",\"result\":",
           object?"true":"false", creator.state == 1 ? nowMs()-started-creator.elapsed_ms : creator.elapsed_ms);
    if (creator.state == 2 || creator.state == 3) resultJson(creator.result); else append("null");
    append("},\"module_scan\":{\"scope\":\"own_process_only\",\"regions\":%u,\"regions_complete\":%s,"
           "\"module_limit\":%s,\"symbol_limit\":%s,\"query_result\":",
           inventory.regions_visited, inventory.regions_complete?"true":"false",
           inventory.module_limit?"true":"false", inventory.symbol_limit?"true":"false");
    resultJson(inventory.query_result); append(",\"modules\":[");
    for (size_t i = 0; i < inventory.module_count; i++) {
        ProbeModule *m = &inventory.modules[i]; if (i) append(",");
        append("{\"base\":\"0x%" PRIx64 "\",\"dynamic\":\"0x%" PRIx64 "\",\"symbol_count\":%u,"
               "\"matched_count\":%u,\"symbols_complete\":%s,\"status\":",
               m->base, m->dynamic, m->symbol_count, m->matched_count, m->symbols_complete?"true":"false");
        quoted(m->status); append("}");
    }
    append("],\"symbols\":[");
    for (size_t i = 0; i < inventory.symbol_count; i++) {
        ProbeSymbol *s = &inventory.symbols[i]; if (i) append(",");
        append("{\"name\":"); quoted(s->name);
        append(",\"address\":\"0x%" PRIx64 "\",\"size\":%" PRIu64 ",\"defined\":%s,"
               "\"elf_type\":%u,\"executable_candidate\":%s,\"invoked\":false}",
               s->address, s->size, s->section?"true":"false", s->info&15, s->executable?"true":"false");
    }
    append("]},\"limits\":[\"Opening a service does not establish its hosting ABI or authorization.\","
           "\"SM errors do not by themselves establish global service absence.\","
           "\"Automatic baseline is disabled in agent mode; remote native commands and their results are recorded separately in /agent.\","
           "\"Only own-process MOD0 modules with SysV hash counts are searched; other processes are unobserved.\","
           "\"A missing SDK export in this process does not establish missing system functionality.\"]}\n");
    if (report_overflow) {
        snprintf(report, sizeof(report), "{\"schema\":\"gameshare-probe-v1\",\"error\":\"report_overflow\"}\n");
        report_length = strlen(report);
    }
    return gen;
}
static void persistReport(void) {
    if (!sd_mounted) return;
    FILE *out = fopen("sdmc:/switch/gameshare-probe/report.json", "wb");
    if (!out) { persistence_errno = errno; return; }
    size_t n = fwrite(report, 1, report_length, out);
    int close_result = fclose(out);
    persistence_errno = n == report_length && close_result == 0 ? 0 : (errno ? errno : EIO);
}
static void persistAgent(void) {
    if (!sd_mounted) return;
    const char *text=agentStatus();
    FILE *file=fopen("sdmc:/switch/gameshare-probe/agent-last.json","wb");
    if (file) { fwrite(text,1,strlen(text),file); fclose(file); }
}
static bool sendAll(int fd, const char *data, size_t n, uint64_t deadline) {
    while (n && nowMs() < deadline) {
        ssize_t sent = send(fd, data, n, 0);
        if (sent <= 0) return false;
        data += sent; n -= (size_t)sent;
    }
    return n == 0;
}
static bool headerValue(const char *request, const char *name, char *out, size_t capacity) {
    size_t name_size=strlen(name);
    const char *line=strstr(request,"\r\n");
    while (line) {
        line+=2;
        if (!*line || !strncmp(line,"\r\n",2)) break;
        const char *end=strstr(line,"\r\n"); if (!end) break;
        if ((size_t)(end-line)>name_size && !strncasecmp(line,name,name_size) && line[name_size]==':') {
            const char *value=line+name_size+1;
            while (value<end && (*value==' ' || *value=='\t')) value++;
            while (end>value && (end[-1]==' ' || end[-1]=='\t')) end--;
            if ((size_t)(end-value)>=capacity) return false;
            memcpy(out,value,(size_t)(end-value)); out[end-value]=0; return true;
        }
        line=end;
    }
    return false;
}
static void answer(int fd) {
    struct timeval timeout = {.tv_sec=1};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    char request[4096]; size_t used = 0; uint64_t deadline = nowMs()+3000;
    request[0] = 0;
    while (used < sizeof(request)-1 && nowMs() < deadline && !strstr(request, "\r\n\r\n")) {
        ssize_t n = recv(fd, request+used, sizeof(request)-1-used, 0);
        if (n <= 0) break;
        used += (size_t)n; request[used] = 0;
    }
    const char *body = "{\"error\":\"bad_request\"}\n", *status = "400 Bad Request";
    size_t length = strlen(body);
    if (strstr(request, "\r\n\r\n")) {
        bool health=!strncmp(request,"GET /health HTTP/1.",19);
        bool get_report=!strncmp(request,"GET /report HTTP/1.",19);
        bool get_agent=!strncmp(request,"GET /agent HTTP/1.",18);
        bool run=!strncmp(request,"POST /run HTTP/1.",17);
        char key[64];
        bool authorized=headerValue(request,"X-Probe-Key",key,sizeof(key)) && !strcmp(key,access_key);
        if (health) {
            body="{\"app\":\"gameshare-probe\",\"version\":\"" VERSION "\",\"agent_abi\":1}\n"; status="200 OK";
        } else if ((get_report || get_agent || run) && !authorized) {
            body="{\"error\":\"pairing_key_required\"}\n"; status="403 Forbidden";
        } else if (get_report) { makeReport(); body=report; status="200 OK"; }
        else if (get_agent) { body=agentStatus(); status="200 OK"; }
        else if (run) {
            char size_text[32], transfer[64]; size_t size=0; bool valid=true;
            if (!headerValue(request,"Content-Length",size_text,sizeof(size_text)) || !size_text[0] ||
                headerValue(request,"Transfer-Encoding",transfer,sizeof(transfer))) valid=false;
            if (valid) for (const char *p=size_text;*p;p++) {
                if (*p<'0' || *p>'9' || size>(RESEARCH_CODE_LIMIT+RESEARCH_INPUT_LIMIT+AGENT_HEADER_SIZE)/10) { valid=false; break; }
                size=size*10+(unsigned)(*p-'0');
            }
            if (size<AGENT_HEADER_SIZE || size>RESEARCH_CODE_LIMIT+RESEARCH_INPUT_LIMIT+AGENT_HEADER_SIZE) valid=false;
            if (valid) {
                uint8_t *packet=malloc(size);
                if (packet) {
                    const char *start=strstr(request,"\r\n\r\n")+4;
                    size_t received=used-(size_t)(start-request);
                    if (received>size) valid=false;
                    else memcpy(packet,start,received);
                    deadline=nowMs()+10000;
                    while (valid && received<size && nowMs()<deadline) {
                        ssize_t n=recv(fd,packet+received,size-received,0);
                        if (n<=0) { valid=false; break; }
                        received+=(size_t)n;
                    }
                    int code=valid && received==size ? agentSubmit(packet,size) : 400;
                    free(packet);
                    if (code==202) { body=agentStatus(); status="202 Accepted"; }
                    else if (code==409) { body="{\"error\":\"agent_busy_or_cleanup_failed\"}\n"; status="409 Conflict"; }
                    else if (code==503) { body=agentStatus(); status="503 Service Unavailable"; }
                }
            }
        } else { body = "{\"error\":\"unknown_endpoint\"}\n"; status = "404 Not Found"; }
        length = strlen(body);
    }
    char header[256];
    int h = snprintf(header, sizeof(header), "HTTP/1.0 %s\r\nContent-Type: application/json\r\n"
                     "Content-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n", status, length);
    deadline = nowMs()+3000;
    if (sendAll(fd, header, h, deadline)) sendAll(fd, body, length, deadline);
}

static int createListener(void) {
    struct in_addr ip={.s_addr=(uint32_t)gethostid()};
    inet_ntop(AF_INET,&ip,ip_text,sizeof(ip_text));
    int server=socket(AF_INET,SOCK_STREAM,0);
    if (server>=0) {
        int reuse=1; setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
        struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(PORT),.sin_addr={.s_addr=INADDR_ANY}};
        if (bind(server,(struct sockaddr *)&address,sizeof(address)) || listen(server,2)) {
            server_errno=errno; close(server); server=-1;
        } else server_errno=0;
    } else server_errno=errno;
    server_listening=server>=0;
    return server;
}
static int resetNetwork(int server) {
    if (server>=0) close(server);
    if (R_SUCCEEDED(network_result)) socketExit();
    network_result=socketInitializeDefault();
    server_listening=false;
    return R_SUCCEEDED(network_result)?createListener():-1;
}
int main(void) {
    started = nowMs();
    sd_mounted = fsdevGetDeviceFileSystem("sdmc") != NULL;
    if (!sd_mounted) sd_mounted = sd_owned = R_SUCCEEDED(fsdevMountSdmc());
    if (sd_mounted) {
        mkdir("sdmc:/switch", 0777); mkdir("sdmc:/switch/gameshare-probe", 0777);
    }
    PrintConsole *screen = consoleInit(NULL);
    if (screen) { printf("GameShare Probe " VERSION "\nInspecting own modules...\n"); consoleUpdate(NULL); }
    scanModules();
    SetSysFirmwareVersion version = {0};
    firmware_result = setsysInitialize();
    if (R_SUCCEEDED(firmware_result)) {
        firmware_result = setsysGetFirmwareVersion(&version);
        if (R_SUCCEEDED(firmware_result)) snprintf(firmware, sizeof(firmware), "%u.%u.%u", version.major, version.minor, version.micro);
        setsysExit();
    }
    uint64_t key; randomGet(&key, sizeof(key)); snprintf(access_key, sizeof(access_key), "%016" PRIx64, key);
    if (sd_mounted) {
        FILE *file=fopen("sdmc:/switch/gameshare-probe/agent-key.txt","rb");
        if (file) {
            char configured[18]={0}; size_t n=fread(configured,1,sizeof(configured),file); fclose(file);
            bool valid=n==16 || (n==17 && configured[16]=='\n');
            for (size_t i=0;i<16;i++) if (!((configured[i]>='0' && configured[i]<='9') ||
                (configured[i]>='a' && configured[i]<='f'))) valid=false;
            if (valid) { memcpy(access_key,configured,16); access_key[16]=0; }
        }
    }
    network_result = socketInitializeDefault();
    int server = R_SUCCEEDED(network_result)?createListener():-1;
    server_listening = server >= 0;
    bool baseline_created=false;
    worker_done=true;
    if (RUN_BASELINE) {
        worker_done=false;
        worker_result = threadCreate(&worker, serviceWorker, NULL, NULL, 0x10000, 0x2c, -2);
        if (R_SUCCEEDED(worker_result)) {
            worker_result = threadStart(&worker);
            if (R_FAILED(worker_result)) { threadClose(&worker); worker_done = true; }
            else baseline_created=true;
        } else worker_done = true;
    }
    PadState pad; padConfigureInput(1, HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad);
    unsigned saved_generation = UINT32_MAX; uint64_t redraw = 0, saved_agent_revision=UINT64_MAX;
    bool exit_requested = false, network_reset=false;
    while (appletMainLoop()) {
        unsigned gen = makeReport();
        if (gen != saved_generation) { persistReport(); saved_generation = gen; }
        agentPoll();
        if ((agentTakeNetworkReset() || network_reset) && !agentBusy()) {
            server=resetNetwork(server); network_reset=false;
        }
        uint64_t revision=agentRevision();
        if (revision!=saved_agent_revision) { persistAgent(); saved_agent_revision=revision; }
        if (server >= 0) {
            fd_set fds; FD_ZERO(&fds); FD_SET(server, &fds); struct timeval timeout = {.tv_usec=50000};
            if (select(server+1, &fds, NULL, NULL, &timeout) > 0) {
                int client = accept(server, NULL, NULL);
                if (client >= 0) { answer(client); close(client); }
            }
        } else svcSleepThread(50000000);
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) exit_requested = true;
        if ((padGetButtonsDown(&pad) & HidNpadButton_X) && !agentBusy()) network_reset=true;
        mutexLock(&result_mutex); bool done = worker_done; mutexUnlock(&result_mutex);
        if (exit_requested && done && !agentBusy()) break;
        if (screen && nowMs() >= redraw) {
            consoleClear();
            printf("GameShare Probe " VERSION "   Firmware %s\n\n", firmware);
            printf("PC endpoint: %s:%d  %s\nPairing key: %s\n\n", ip_text, PORT, server>=0?"listening":"unavailable", access_key);
            printf("Native research agent: %s\n",agentBusy()?"job running (status available)":"ready for JIT commands");
            char summary[192]; agentSummary(summary,sizeof(summary)); printf("%s\n\n",summary);
            char tail[768]; agentLogTail(tail,sizeof(tail));
            if (*tail) printf("Recent native log:\n%s\n",tail);
            printf("X: reconnect PC network when idle\n");
            printf("Own MOD0 modules: %zu   matching symbols: %zu\n", inventory.module_count, inventory.symbol_count);
            if (RUN_BASELINE) {
            mutexLock(&result_mutex);
            for (size_t i = 0; i < SERVICE_COUNT; i++) {
                ServiceResult *s = &service_results[i];
                const char *states[] = {"waiting", "pending", "opened", "error"};
                printf("%-8s %-16s %08x\n", service_names[i], states[s->state], s->result);
            }
            mutexUnlock(&result_mutex);
            mutexLock(&result_mutex);
            ServiceResult creator = streamplay_creator;
            bool object = streamplay_object;
            mutexUnlock(&result_mutex);
            printf("\nStreamPlay cmd0: %s  %08x\n", object?"object created + closed":creator.state==1?"pending":creator.state==3?"error":"not attempted", creator.result);
            }
            printf("\nReport: sdmc:/switch/gameshare-probe/report.json\n");
            printf("Agent journal: sdmc:/switch/gameshare-probe/agent-last.json\n");
            printf("%s\n", done && !agentBusy()?"Plus: return to HBMenu":"Command active. Plus queues exit after it completes.");
            if (!done && nowMs()-started > 15000) printf("If pending persists, close the title from HOME.\n");
            if (exit_requested && !done) printf("Exit queued; retaining the active IPC worker.\n");
            consoleUpdate(NULL); redraw = nowMs()+500;
        }
    }
    // Never return to an NRO loader with a live thread executing this NRO's text.
    mutexLock(&result_mutex); bool done = worker_done; mutexUnlock(&result_mutex);
    if (!done || agentBusy()) svcExitProcess();
    if (baseline_created) { threadWaitForExit(&worker); threadClose(&worker); }
    agentShutdown(); persistAgent();
    makeReport(); persistReport();
    if (server >= 0) close(server);
    if (R_SUCCEEDED(network_result)) socketExit();
    if (screen) consoleExit(NULL);
    if (sd_owned) fsdevUnmountDevice("sdmc");
    return 0;
}
