#include "BoxArtManager.hpp"
#include "Data.hpp"
#include "Settings.hpp"
#include "nanovg.h"
#include <algorithm>
#include <cctype>
#include <mutex>
#include <CImg.h>
#include <filesystem>

std::mutex m_mutex;

namespace fs = std::filesystem;

namespace {
// Files are named <host>-<app id>.png, with the host's MAC address (or its
// address when the MAC is unknown) reduced to letters, digits and dots.
std::string host_prefix(const Host& host) {
    std::string owner = host.mac.empty() ? host.preferred_address() : host.mac;
    for (char& c : owner) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.')
            c = '_';
    }
    return owner + "-";
}

std::string boxart_path(const Host& host, int app_id) {
    return (fs::path(Settings::instance().boxart_dir()) /
            (host_prefix(host) + std::to_string(app_id) + ".png"))
            .string();
}

bool is_app_id_png(const std::string& name, size_t offset) {
    if (name.size() < offset + 5)
        return false;
    const size_t end = name.size() - 4;
    if (name.compare(end, 4, ".png") != 0)
        return false;
    return std::all_of(name.begin() + offset, name.begin() + end,
                       [](char c) { return std::isdigit((unsigned char)c); });
}

template <typename Predicate>
void remove_boxart_files(Predicate matches) {
    std::error_code error;
    fs::directory_iterator it(Settings::instance().boxart_dir(), error);
    for (; !error && it != fs::directory_iterator(); it.increment(error)) {
        if (matches(it->path().filename().string())) {
            std::error_code ignored;
            fs::remove(it->path(), ignored);
        }
    }
}
}

BoxArtManager::BoxArtManager() {
    // Box art used to be keyed by app id only and shared between hosts.
    remove_boxart_files(
        [](const std::string& name) { return is_app_id_png(name, 0); });
}

bool BoxArtManager::has_boxart(const Host& host, int app_id) {
    std::string path = boxart_path(host, app_id);
    if (m_has_boxart.count(path)) {
        return m_has_boxart[path];
    }

    Data data = Data::read_from_file(path);
    m_has_boxart[path] = !data.is_empty();

    return m_has_boxart[path];
}

void BoxArtManager::set_data(Data data, const Host& host, int app_id) {
    std::lock_guard<std::mutex> guard(m_mutex);

    std::string path = boxart_path(host, app_id);

    data.write_to_file(path);
    compress_texture(path);

    m_has_boxart[path] = true;
}

void BoxArtManager::compress_texture(const std::string& path) {
    using namespace cimg_library;

    /*
     * target width == 300
     * target height == 400
     * 0.75 == 300 / 400
     */

    CImg<unsigned char> pic(path.c_str());
    if (float(pic.width()) / float(pic.height()) < 0.75f) {
        pic = pic.resize(300, int(float(pic.height()) * 300.0f / float(pic.width())), 1, 3);
    } else {
        pic = pic.resize(int(float(pic.width()) * 400.0f / float(pic.height())), 400, 1, 3);
    }
    pic.save(path.c_str());
}

std::string BoxArtManager::get_texture_path(const Host& host, int app_id) {
    return boxart_path(host, app_id);
}

void BoxArtManager::make_texture_from_boxart(NVGcontext* ctx, const Host& host,
                                             int app_id) {
    std::lock_guard<std::mutex> guard(m_mutex);

    std::string path = boxart_path(host, app_id);
    Data data = Data::read_from_file(path);

    if (data.is_empty()) {
        m_has_boxart[path] = false;
        return;
    }

    int handle = nvgCreateImageMem(ctx, 0, data.bytes(), (int)data.size());

    if (handle > 0) {
        m_texture_handle[path] = handle;
    } else {
        m_has_boxart[path] = false;
    }
}

int BoxArtManager::texture_id(const Host& host, int app_id) {
    std::string path = boxart_path(host, app_id);
    if (m_texture_handle.count(path)) {
        return m_texture_handle[path];
    }
    return -1;
}

void BoxArtManager::remove_host(const Host& host) {
    std::lock_guard<std::mutex> guard(m_mutex);

    const std::string prefix = host_prefix(host);
    remove_boxart_files([&prefix](const std::string& name) {
        return name.compare(0, prefix.size(), prefix) == 0 &&
               is_app_id_png(name, prefix.size());
    });

    const std::string path_prefix =
        (fs::path(Settings::instance().boxart_dir()) / prefix).string();
    auto owned = [&path_prefix](const auto& item) {
        return item.first.compare(0, path_prefix.size(), path_prefix) == 0;
    };
    std::erase_if(m_has_boxart, owned);
    std::erase_if(m_texture_handle, owned);
}
