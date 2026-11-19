#pragma once

#include <array>
#include <cstdint>
#include <vector>

struct StreamMode
{
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t fps    = 0;
    bool docked     = false;

    bool operator==(const StreamMode& other) const
    {
        return width == other.width && height == other.height && fps == other.fps &&
               docked == other.docked;
    }
    bool operator!=(const StreamMode& other) const { return !(*this == other); }
};

// One second of a stream. Upstream has no count of late decoder wakes, so
// only arrivals, presented frames, frames skipped on the console and frames
// lost on the network are judged.
struct StreamSecond
{
    uint64_t arrived   = 0;
    uint64_t presented = 0;
    uint64_t skipped   = 0;
    uint64_t linkLost  = 0;
};

// Battery saver clock plan for a stream mode, and the governor that puts the
// clocks back to stock as soon as the stream falls behind.
class PowerGovernor
{
  public:
    enum Clock { CPU, GPU, EMC, CLOCKS };
    using Rates = std::array<uint32_t, CLOCKS>;

    static constexpr int behindSeconds   = 3;
    static constexpr int recoverySeconds = 2;

    static bool keepsStock(const StreamMode& mode);
    static Rates plan(const StreamMode& mode);
    static uint32_t snap(uint32_t targetHz, uint32_t stockHz, const std::vector<uint32_t>& rates);
    static bool behind(const StreamSecond& second, uint32_t fps, bool recovering);

    void hold();
    bool second(const StreamSecond& second, uint32_t fps);

  private:
    int behindCount      = 0;
    int secondsSinceLoss = -1;
};
