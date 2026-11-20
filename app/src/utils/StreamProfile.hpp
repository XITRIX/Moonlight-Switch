#pragma once
struct StreamProfile {
    int resolution = 720;
    int fps = 60;
    int bitrate = 10000;
    bool maximum = false;
};
class StreamProfileLatch {
public:
    void begin(const StreamProfile& profile) { if (!active) { value = profile; active = true; } }
    void end() { active = false; }
    bool active = false;
    StreamProfile value;
};
