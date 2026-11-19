#pragma once

#include <string>

// Keeps a copy of every borealis log line in a file, one file per launch.
class LogFile {
  public:
    // Lines logged before open() are held in memory until the file exists.
    static void start();

    // Opens a fresh log at path and keeps the logs of the previous launches
    // next to it as name.1.ext, name.2.ext and so on.
    static void open(const std::string& path);

    // Copies the current log into directory under a dated name. Returns the
    // new file's path, or an empty string when nothing could be copied.
    static std::string save_copy(const std::string& directory);
};
