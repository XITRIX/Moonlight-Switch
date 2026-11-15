// Read this process's identity and retain the last local account UID privately.
#include <switch.h>
#include "research_api.h"
uint64_t probe_entry(const ResearchApi *api,ResearchContext *ctx) {
    if(ctx->state_size<800)return 1;
    Result (*info)(u64*,u32,Handle,u64)=(void*)api->resolve("svcGetInfo");
    Result (*pid)(u64*,Handle)=(void*)api->resolve("svcGetProcessId");
    u64 (*aruid)(void)=(void*)api->resolve("appletGetAppletResourceUserId");
    if(!info||!pid||!aruid)return 2;
    u64 value=0;api->log_value("pid_result",pid(&value,CUR_PROCESS_HANDLE));api->log_value("pid",value);
    value=0;api->log_value("program_id_result",info(&value,InfoType_ProgramId,CUR_PROCESS_HANDLE,0));
    api->log_value("program_id",value);api->log_value("applet_resource_user_id",aruid());
    uint32_t account=0,rc=api->service_open("acc:u0",&account);
    api->log_value("account_open_result",rc);if(rc)return rc;
    uint64_t uid[2]={0};ResearchReply reply={0};
    ResearchIpc req={.command=4,.output=uid,.output_size=16};
    rc=api->ipc(account,&req,&reply);api->log_value("last_user_result",rc);
    if(!rc) {
        __builtin_memcpy(ctx->state+768,uid,16);ctx->state[784]=(uid[0]|uid[1])!=0;
        api->log_value("local_user_present",ctx->state[784]);
    }
    api->log_value("account_close_result",api->service_close(account));
    return rc;
}
