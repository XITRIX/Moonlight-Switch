#pragma once
#ifdef __SWITCH2__
#include <switch.h>

#include <chrono>
#include <vector>

#include "AVFrameHolder.hpp"
#include "PowerGovernor.hpp"

// Lowers the CPU, GPU and memory clocks while a battery saver stream runs and
// hands them back to the system when it ends or falls behind for good.
class Switch2Clocks
{
  public:
    ~Switch2Clocks();

    void update(const StreamMode& mode, bool lower, bool overlay, const FrameCounter& counter,
                uint32_t networkDropped);
    void release(const char* reason);

  private:
    bool open();
    void lowerClocks(const StreamMode& mode);
    void reassert();
    void govern(bool overlay, const FrameCounter& counter, uint32_t networkDropped);
    void restoreStock(const StreamSecond& second);

    ClkrstSession sessions[PowerGovernor::CLOCKS] = {};
    std::vector<uint32_t> possibleRates[PowerGovernor::CLOCKS];
    bool opened       = false;
    bool unavailable  = false;
    bool lowered      = false;
    bool holding      = false;
    bool settling     = false;
    int profileLogged = -1;
    StreamMode mode;
    std::chrono::steady_clock::time_point modeSince, lastRead;
    PowerGovernor::Rates stock = {}, current = {};
    PowerGovernor governor;
    uint64_t lastSecond = 0;
    uint32_t lastNetworkDropped = 0;
};
#endif
