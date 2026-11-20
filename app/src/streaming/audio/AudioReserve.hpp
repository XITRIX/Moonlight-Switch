#pragma once
class AudioReserve {
public:
    void reset(int milliseconds) { target = milliseconds; buffering = true; }
    bool underrun(int queuedMs) {
        if (target > 0 && queuedMs == 0 && !buffering) { buffering = true; return true; }
        return false;
    }
    bool ready(int queuedMs) {
        if (buffering && queuedMs >= target) buffering = false;
        return !buffering;
    }
private:
    int target = 10;
    bool buffering = true;
};
