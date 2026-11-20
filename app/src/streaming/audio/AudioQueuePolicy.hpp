#pragma once
struct AudioQueuePolicy {
    enum Drop { None, InputBacklog, OutputBacklog, NoBuffer };
    static Drop decide(int reserve, int pending, int queued, bool freeBuffer) {
        // The intentional reserve is not excess latency. Leave 30 ms of burst
        // headroom, but never permit the renderer queue to grow without bound.
        if (queued >= reserve + 30) return OutputBacklog;
        // Do not starve playback just because compressed packets arrived late.
        if (pending > 30 && queued >= reserve + 10 && pending + queued > reserve + 45)
            return InputBacklog;
        return freeBuffer ? None : NoBuffer;
    }
    static float pitch(int reserve, int queued) {
        // Gradually drain excess before reaching the emergency discard limit.
        // A temporary correction changes pitch slightly; bound it to 2%.
        const int excess = queued - reserve;
        if (excess > 3) return 1.0f + (excess < 20 ? excess : 20) * 0.001f;
        return 1.0f;
    }
    static float slew(float current, float target) {
        const float step = 0.0005f;
        if (target > current + step) return current + step;
        if (target < current - step) return current - step;
        return target;
    }
};
