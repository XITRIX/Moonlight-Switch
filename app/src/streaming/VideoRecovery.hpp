#pragma once
#include <cstdint>

// Presentation progress, independent of decoder statistics that may stop updating.
class VideoRecovery {
public:
    enum Action { None, Keyframe, Reconnect };
    void begin(uint64_t now) { window = now; shown = 0; requested = false; }
    Action update(uint64_t now, bool consumed, bool poor) {
        if (consumed) ++shown;
        if (now < window) { begin(now); return None; }
        const auto elapsed = now - window;
        if (elapsed < 5000) return None;
        // Require sustained poor delivery, not a single late frame or static desktop.
        if (!poor || shown >= elapsed / 100) { begin(now); return None; }
        if (!requested) { requested = true; return Keyframe; }
        if (elapsed >= 8000) {
            begin(now);
            if (retries < 2) { ++retries; return Reconnect; }
        }
        return None;
    }
private:
    uint64_t window = 0;
    unsigned shown = 0, retries = 0;
    bool requested = false;
};
