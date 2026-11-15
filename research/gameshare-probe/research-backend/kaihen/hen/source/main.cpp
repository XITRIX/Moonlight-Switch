// SPDX-License-Identifier: GPL-2.0-only
#include <switch.h>
#include "server.hpp"

extern "C" {
u32 __nx_applet_type = AppletType_None;
size_t __nx_heap_size = 0x200000;

void __appInit(void) {
}

void __appExit(void) {
}
}

int main(int argc, char** argv) {
    NX_IGNORE_ARG(argc);
    NX_IGNORE_ARG(argv);
    Result rc = smInitialize();
    if (R_FAILED(rc)) {
        return static_cast<int>(rc);
    }
    {
        HenServer server;
        rc = server.initialize();
        if (R_SUCCEEDED(rc)) {
            rc = server.run();
        }
    }
    smExit();
    return static_cast<int>(rc);
}
