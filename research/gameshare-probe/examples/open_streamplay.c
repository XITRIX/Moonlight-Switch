// Retains the root service in state[8..11] for subsequent commands.
#include "research_api.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *context) {
    uint32_t *root=(uint32_t *)(context->state+8);
    if (*root) { api->log_value("existing_root_id",*root); return 0; }
    uint32_t result=api->service_open("stpl:u",root);
    api->log_value("SM_result",result);
    api->log_value("root_id",*root);
    return result;
}
