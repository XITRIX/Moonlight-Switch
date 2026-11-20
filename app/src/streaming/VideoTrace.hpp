#pragma once
#include <atomic>
#include <cstdint>
namespace videoTrace {
inline std::atomic<uint64_t> submitted{0};
}
