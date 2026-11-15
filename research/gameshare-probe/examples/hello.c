// This first command qualifies upload -> JIT publication -> native execution -> result.
#include "research_api.h"
uint64_t probe_entry(const ResearchApi *api, ResearchContext *context) {
    api->log("Native command executed");
    uint64_t *counter=(uint64_t *)context->state;
    (*counter)++;
    api->log_value("persistent_counter",*counter);
    context->output[0]=42; context->output_size=1;
    return 42;
}
