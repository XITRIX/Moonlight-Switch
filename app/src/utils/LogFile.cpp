#include "LogFile.hpp"

#include <borealis.hpp>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <vector>

#ifndef APP_COMMIT
#define APP_COMMIT "unknown"
#endif

namespace fs = std::filesystem;

namespace {

constexpr int kept_logs = 3;
constexpr size_t max_early_lines = 2000;

std::mutex log_mutex;
std::FILE* log_file = nullptr;
std::string log_path;
std::vector<std::string> early_lines;
bool collecting = false;

const char* level_name(brls::LogLevel level) {
    switch (level) {
        case brls::LogLevel::LOG_ERROR:
            return "ERROR";
        case brls::LogLevel::LOG_WARNING:
            return "WARNING";
        case brls::LogLevel::LOG_INFO:
            return "INFO";
        case brls::LogLevel::LOG_DEBUG:
            return "DEBUG";
        default:
            return "VERBOSE";
    }
}

std::string format_line(brls::Logger::TimePoint time, brls::LogLevel level,
                        const std::string& message) {
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(time.time_since_epoch()).count() % 1000;
    std::time_t seconds = system_clock::to_time_t(time);
    std::tm local = *std::localtime(&seconds);
    return fmt::format("{:%H:%M:%S}.{:03d} [{}] {}\n", local, (int)ms,
                       level_name(level), message);
}

void write_line(const std::string& line) {
    std::fputs(line.c_str(), log_file);
    std::fflush(log_file);
}

fs::path numbered(const fs::path& path, int number) {
    return path.parent_path() / (path.stem().string() + "." +
                                 std::to_string(number) +
                                 path.extension().string());
}

void rotate(const fs::path& path) {
    std::error_code error;
    fs::remove(numbered(path, kept_logs), error);
    for (int number = kept_logs; number > 0; number--) {
        fs::path from = number == 1 ? path : numbered(path, number - 1);
        fs::rename(from, numbered(path, number), error);
    }
}

std::string time_stamp(const char* format) {
    std::time_t now = std::time(nullptr);
    char text[32] = "";
    if (std::tm* local = std::localtime(&now))
        std::strftime(text, sizeof(text), format, local);
    return text;
}

bool copy_file(const std::string& from, const std::string& to) {
    std::FILE* source = std::fopen(from.c_str(), "rb");
    if (!source)
        return false;
    std::FILE* target = std::fopen(to.c_str(), "wb");
    bool copied = target != nullptr;
    char buffer[4096];
    size_t count;
    while (copied && (count = std::fread(buffer, 1, sizeof(buffer), source)) > 0)
        copied = std::fwrite(buffer, 1, count, target) == count;
    std::fclose(source);
    return target && std::fclose(target) == 0 && copied;
}

} // namespace

void LogFile::start() {
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        collecting = true;
    }
    brls::Logger::getLogEvent()->subscribe(
        [](brls::Logger::TimePoint time, brls::LogLevel level,
           std::string message) {
            std::string line = format_line(time, level, message);
            std::lock_guard<std::mutex> lock(log_mutex);
            if (log_file)
                write_line(line);
            else if (collecting && early_lines.size() < max_early_lines)
                early_lines.push_back(line);
        });
}

void LogFile::open(const std::string& path) {
    std::string platform = "unknown";
    if (auto* current = brls::Application::getPlatform())
        platform = current->getName();

    std::lock_guard<std::mutex> lock(log_mutex);
    if (log_file)
        return;

    rotate(fs::path(path));
    log_file = std::fopen(path.c_str(), "w");
    collecting = false;
    if (log_file) {
        log_path = path;
        write_line(fmt::format("Moonlight {} commit {} on {}, started {}\n",
                               APP_VERSION, APP_COMMIT, platform,
                               time_stamp("%Y-%m-%d %H:%M:%S")));
        for (const auto& line : early_lines)
            write_line(line);
    }
    early_lines.clear();
    early_lines.shrink_to_fit();
}

std::string LogFile::save_copy(const std::string& directory) {
    std::lock_guard<std::mutex> lock(log_mutex);
    if (!log_file)
        return "";

    std::fflush(log_file);
    std::error_code error;
    fs::create_directories(fs::path(directory), error);
    fs::path target = fs::path(directory) /
                      ("moonlight-" + time_stamp("%Y%m%d-%H%M%S") + ".log");
    target.make_preferred();
    return copy_file(log_path, target.string()) ? target.string() : "";
}
