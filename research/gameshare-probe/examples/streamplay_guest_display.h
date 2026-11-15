#pragma once
#include <switch2/services/vi2.h>
#include "own_vi2_symbols.h"
typedef struct {
    Vi2Display display; Vi2Endpoint endpoint; Vi2Layer layer;
    bool opened,created,attached,selected;
} GuestDisplay;
static uint32_t guest_display_open(const ResearchApi *api,GuestDisplay *g) {
    if(g->opened || g->created)return 10;
    uintptr_t fn[11]={0};
    for(unsigned i=1;i<11;i++) { fn[i]=own_vi2_symbol(api,i);if(!fn[i])return 11; }
    uint32_t rc=((Result (*)(Vi2Display*,const char*))fn[1])(&g->display,"player1");
    api->log_value("open_player1_display",rc);if(rc)return rc;g->opened=true;
    ((void (*)(Vi2Endpoint*,Vi2Display*,const char*))fn[3])(&g->endpoint,&g->display,"default");
    rc=((Result (*)(Vi2Layer*))fn[5])(&g->layer);
    api->log_value("create_guest_layer",rc);if(rc)return rc;g->created=true;
    rc=((Result (*)(Vi2Layer*,Vi2Display*))fn[6])(&g->layer,&g->display);
    api->log_value("attach_guest_layer",rc);if(rc)return rc;g->attached=true;
    rc=((Result (*)(Vi2Layer*,Vi2Endpoint*))fn[7])(&g->layer,&g->endpoint);
    api->log_value("select_guest_layer",rc);if(rc)return rc;g->selected=true;
    return 0;
}
static uint32_t guest_display_close(const ResearchApi *api,GuestDisplay *g) {
    uint32_t rc=0;
    if(g->attached) {
        uintptr_t fn=own_vi2_symbol(api,9);if(!fn)return 11;
        rc=((Result (*)(Vi2Layer*))fn)(&g->layer);
        api->log_value("detach_guest_layer",rc);if(rc)return rc;g->attached=false;
    }
    if(g->selected) {
        uintptr_t fn=own_vi2_symbol(api,8);if(!fn)return 11;
        rc=((Result (*)(Vi2Endpoint*))fn)(&g->endpoint);
        api->log_value("clear_guest_selection",rc);if(rc)return rc;g->selected=false;
    }
    if(g->created) {
        uintptr_t fn=own_vi2_symbol(api,10);if(!fn)return 11;
        rc=((Result (*)(Vi2Layer*))fn)(&g->layer);
        api->log_value("close_guest_layer",rc);if(rc)return rc;g->created=false;
    }
    if(g->opened) {
        uintptr_t ep=own_vi2_symbol(api,4),display=own_vi2_symbol(api,2);if(!ep||!display)return 11;
        ((void (*)(Vi2Endpoint*))ep)(&g->endpoint);
        rc=((Result (*)(Vi2Display*))display)(&g->display);
        api->log_value("close_guest_display",rc);if(rc)return rc;g->opened=false;
    }
    return 0;
}
