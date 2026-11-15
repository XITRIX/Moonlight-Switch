#include "research_api.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *context) {
    uint32_t *root=(uint32_t *)(context->state+8), *scoped=(uint32_t *)(context->state+12);
    uint32_t result=0;
    if (*scoped) { result=api->service_close(*scoped); api->log_value("close_scoped",result); if (result) return result; *scoped=0; }
    if (*root) { result=api->service_close(*root); api->log_value("close_root",result); if (!result) *root=0; }
    return result;
}
