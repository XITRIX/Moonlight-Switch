#ifdef __SWITCH2__
#include "Switch2Clocks.hpp"

#include <borealis.hpp>

#include <algorithm>

namespace
{
constexpr PcvModuleId modules[PowerGovernor::CLOCKS] = { PcvModuleId_CpuBus, PcvModuleId_GPU,
                                                         PcvModuleId_EMC };
constexpr const char* names[PowerGovernor::CLOCKS] = { "cpu", "gpu", "emc" };
constexpr auto settleTime   = std::chrono::seconds(2);
constexpr auto reassertTime = std::chrono::seconds(5);
constexpr int maxRates      = 64;

uint32_t mhz(uint32_t hz)
{
    return hz / 1000000;
}
}

Switch2Clocks::~Switch2Clocks()
{
    release("exit");
}

void Switch2Clocks::update(const StreamMode& streamMode, bool lower, bool overlay,
                           const FrameCounter& counter, uint32_t networkDropped)
{
    if (unavailable)
        return;
    if (profileLogged != int(lower))
    {
        profileLogged = int(lower);
        brls::Logger::info("clocks: profile={}", lower ? "saver" : "performance");
    }
    if (!lower)
    {
        release("profile");
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (settling || lowered)
    {
        if (streamMode != mode)
            release(streamMode.docked != mode.docked ? "dock-change" : "mode-change");
    }
    if (!settling && !lowered)
    {
        settling  = true;
        mode      = streamMode;
        modeSince = now;
    }

    if (settling)
    {
        if (now - modeSince >= settleTime)
            lowerClocks(mode);
        return;
    }

    if (now - lastRead >= reassertTime)
        reassert();
    govern(overlay, counter, networkDropped);
}

bool Switch2Clocks::open()
{
    if (opened)
        return true;

    Result rc = clkrstInitialize();
    if (R_FAILED(rc))
    {
        unavailable = true;
        brls::Logger::info("clocks: unavailable rc=0x{:x}", rc);
        return false;
    }
    for (int clock = 0; clock < PowerGovernor::CLOCKS; clock++)
    {
        rc = clkrstOpenSession(&sessions[clock], modules[clock], 3);
        if (R_SUCCEEDED(rc))
            continue;
        while (clock-- > 0)
            clkrstCloseSession(&sessions[clock]);
        clkrstExit();
        unavailable = true;
        brls::Logger::info("clocks: unavailable rc=0x{:x}", rc);
        return false;
    }

    for (int clock = 0; clock < PowerGovernor::CLOCKS; clock++)
    {
        if (!possibleRates[clock].empty())
            continue;
        uint32_t rates[maxRates] = {};
        PcvClockRatesListType type = PcvClockRatesListType_Invalid;
        s32 total = 0;
        Result ratesRc = clkrstGetPossibleClockRates(&sessions[clock], rates, maxRates, &type, &total);
        if (R_SUCCEEDED(ratesRc) && type == PcvClockRatesListType_Discrete)
            possibleRates[clock].assign(rates, rates + std::clamp(total, 0, maxRates));
    }
    opened = true;
    return true;
}

void Switch2Clocks::lowerClocks(const StreamMode& streamMode)
{
    settling = false;
    if (!open())
        return;

    const bool keep      = PowerGovernor::keepsStock(streamMode);
    const auto targets   = PowerGovernor::plan(streamMode);
    Result results[PowerGovernor::CLOCKS] = {};
    for (int clock = 0; clock < PowerGovernor::CLOCKS; clock++)
    {
        stock[clock] = 0;
        clkrstGetClockRate(&sessions[clock], &stock[clock]);
        current[clock] = stock[clock];
        if (keep || stock[clock] == 0)
            continue;

        const uint32_t target =
            PowerGovernor::snap(targets[clock], stock[clock], possibleRates[clock]);
        if (target >= stock[clock])
            continue;
        results[clock] = clkrstSetClockRate(&sessions[clock], target);
        if (R_SUCCEEDED(results[clock]))
            current[clock] = target;
    }

    lowered    = true;
    holding    = false;
    governor   = {};
    lastSecond = 0;
    lastRead   = std::chrono::steady_clock::now();
    brls::Logger::info(
        "clocks: set cpu={} gpu={} emc={} MHz reason={} mode={}x{}@{} docked={} stock={}/{}/{} "
        "results=0x{:x}/0x{:x}/0x{:x}",
        mhz(current[0]), mhz(current[1]), mhz(current[2]), keep ? "keep-stock" : "start",
        streamMode.width, streamMode.height, streamMode.fps, streamMode.docked, mhz(stock[0]),
        mhz(stock[1]), mhz(stock[2]), results[0], results[1], results[2]);
}

void Switch2Clocks::reassert()
{
    lastRead = std::chrono::steady_clock::now();
    for (int clock = 0; clock < PowerGovernor::CLOCKS; clock++)
    {
        uint32_t read = 0;
        if (current[clock] >= stock[clock] ||
            R_FAILED(clkrstGetClockRate(&sessions[clock], &read)) || read < stock[clock])
            continue;
        Result rc = clkrstSetClockRate(&sessions[clock], current[clock]);
        brls::Logger::info("clocks: set {}={} MHz reason=reasserted read={} result=0x{:x}",
                           names[clock], mhz(current[clock]), mhz(read), rc);
    }
}

void Switch2Clocks::govern(bool overlay, const FrameCounter& counter, uint32_t networkDropped)
{
    if (counter.second == lastSecond)
        return;
    const bool first = lastSecond == 0;
    lastSecond       = counter.second;

    StreamSecond second;
    second.arrived     = counter.presented + counter.skipped;
    second.presented   = counter.presented;
    second.skipped     = counter.skipped;
    second.linkLost    = networkDropped >= lastNetworkDropped ? networkDropped - lastNetworkDropped : 0;
    lastNetworkDropped = networkDropped;
    if (first)
        return;

    if (overlay)
    {
        if (!holding)
            brls::Logger::info("clocks: hold reason=overlay");
        holding = true;
        governor.hold();
        return;
    }
    holding = false;

    if (governor.second(second, mode.fps))
        restoreStock(second);
}

void Switch2Clocks::restoreStock(const StreamSecond& second)
{
    Result results[PowerGovernor::CLOCKS] = {};
    bool raised                           = false;
    for (int clock = 0; clock < PowerGovernor::CLOCKS; clock++)
    {
        if (current[clock] >= stock[clock])
            continue;
        results[clock] = clkrstSetClockRate(&sessions[clock], stock[clock]);
        current[clock] = stock[clock];
        raised         = true;
    }
    if (!raised)
        return;

    brls::Logger::info("clocks: set cpu={} gpu={} emc={} MHz reason=behind presented={} "
                       "arrived={} skipped={} link_lost={} results=0x{:x}/0x{:x}/0x{:x}",
                       mhz(current[0]), mhz(current[1]), mhz(current[2]), second.presented,
                       second.arrived, second.skipped, second.linkLost, results[0], results[1],
                       results[2]);
}

void Switch2Clocks::release(const char* reason)
{
    settling = false;
    lowered  = false;
    if (!opened)
        return;

    for (auto& session : sessions)
        clkrstCloseSession(&session);
    clkrstExit();
    opened = false;

    ApmPerformanceMode performanceMode = ApmPerformanceMode_Invalid;
    u32 configuration                  = 0;
    Result rc                          = apmInitialize();
    if (R_SUCCEEDED(rc))
    {
        rc = apmGetPerformanceMode(&performanceMode);
        if (R_SUCCEEDED(rc))
            rc = apmGetPerformanceConfiguration(performanceMode, &configuration);
        if (R_SUCCEEDED(rc))
            rc = apmSetPerformanceConfiguration(performanceMode, configuration);
        apmExit();
    }
    brls::Logger::info("clocks: released reason={} apm=0x{:x} configuration=0x{:08x}", reason, rc,
                       configuration);
}
#endif
