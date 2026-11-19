#include "PowerGovernor.hpp"

#include <algorithm>

namespace
{
constexpr uint64_t presentedPercent = 95;
constexpr uint64_t lostOneIn        = 30;

// MHz by pixel rate: up to 1080p30, 1080p60, 4K30 or 1440p60, and above.
// Docked, the GPU and the memory also carry the TV output.
constexpr PowerGovernor::Rates handheld[] = {
    { 614, 230, 1065 },
    { 714, 307, 1331 },
    { 816, 384, 1600 },
    { 1020, 460, 2133 },
};
constexpr PowerGovernor::Rates docked[] = {
    { 614, 307, 1600 },
    { 714, 384, 2133 },
    { 816, 460, 2665 },
    { 1020, 614, 3200 },
};

int tier(const StreamMode& mode)
{
    const uint64_t pixels = uint64_t(mode.width) * mode.height * mode.fps;
    if (pixels <= 62208000)
        return 0;
    if (pixels <= 124416000)
        return 1;
    if (pixels <= 248832000)
        return 2;
    return 3;
}
}

bool PowerGovernor::keepsStock(const StreamMode& mode)
{
    return mode.docked && mode.fps >= 120;
}

PowerGovernor::Rates PowerGovernor::plan(const StreamMode& mode)
{
    Rates rates = mode.docked ? docked[tier(mode)] : handheld[tier(mode)];
    for (auto& rate : rates)
        rate *= 1000000;
    return rates;
}

uint32_t PowerGovernor::snap(uint32_t targetHz, uint32_t stockHz, const std::vector<uint32_t>& rates)
{
    if (rates.empty())
        return std::min(targetHz, stockHz);

    uint32_t best = stockHz;
    for (uint32_t rate : rates)
    {
        if (rate >= targetHz && rate <= stockHz)
            best = std::min(best, rate);
    }
    return best;
}

bool PowerGovernor::behind(const StreamSecond& second, uint32_t fps, bool recovering)
{
    const uint64_t rate = std::max<uint32_t>(fps, 1);
    const bool full     = second.arrived * 100 >= rate * presentedPercent;
    const bool shortOf  = second.presented * 100 < rate * presentedPercent;
    if (recovering)
        return full && shortOf;

    const bool idle = second.arrived * 2 < rate;
    return (shortOf && !idle) || second.skipped * lostOneIn > rate;
}

void PowerGovernor::hold()
{
    behindCount = 0;
}

bool PowerGovernor::second(const StreamSecond& second, uint32_t fps)
{
    if (second.linkLost > 0)
        secondsSinceLoss = 0;
    else if (secondsSinceLoss >= 0)
        secondsSinceLoss++;

    const bool recovering = secondsSinceLoss >= 0 && secondsSinceLoss <= recoverySeconds;
    behindCount           = behind(second, fps, recovering) ? behindCount + 1 : 0;
    if (behindCount < behindSeconds)
        return false;

    behindCount = 0;
    return true;
}
