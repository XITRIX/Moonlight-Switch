#include "BoxArtManager.hpp"
#include "Data.hpp"
#include "Settings.hpp"
#include "nanovg.h"
#include <algorithm>
#include <mutex>
#include <CImg.h>

std::mutex m_mutex;

bool BoxArtManager::has_boxart(int app_id) {
    if (m_has_boxart.count(app_id)) {
        return m_has_boxart[app_id];
    }

    std::string path = Settings::instance().boxart_dir() + "/" +
                       std::to_string(app_id) + ".png";
    Data data = Data::read_from_file(path);
    m_has_boxart[app_id] = !data.is_empty();

    return m_has_boxart[app_id];
}

void BoxArtManager::set_data(Data data, int app_id) {
    std::lock_guard<std::mutex> guard(m_mutex);

    std::string path = Settings::instance().boxart_dir() + "/" +
                       std::to_string(app_id) + ".png";

    data.write_to_file(path);
    compress_texture(path);

    m_has_boxart[app_id] = true;
}

void BoxArtManager::compress_texture(std::string path) {
    using namespace cimg_library;

    /*
     * target width == 300
     * target height == 400
     * 0.75 == 300 / 400
     */

    CImg<unsigned char> pic(path.c_str());
    if (float(pic.width()) / float(pic.height()) < 0.75f) {
        pic = pic.resize(300, float(pic.height()) * 300.0f / float(pic.width()), 1, 3);
    } else {
        pic = pic.resize(float(pic.width()) * 400.0f / float(pic.height()), 400, 1, 3);
    }
    pic.save(path.c_str());
}

std::string BoxArtManager::get_texture_path(int app_id) {
    return Settings::instance().boxart_dir() + "/" + std::to_string(app_id) +
           ".png";
}

void BoxArtManager::make_texture_from_boxart(NVGcontext* ctx, int app_id) {
    std::lock_guard<std::mutex> guard(m_mutex);

    std::string path = Settings::instance().boxart_dir() + "/" +
                       std::to_string(app_id) + ".png";
    Data data = Data::read_from_file(path);

    if (data.is_empty()) {
        m_has_boxart[app_id] = false;
        return;
    }

    int handle = nvgCreateImageMem(ctx, 0, data.bytes(), (int)data.size());

    if (handle > 0) {
        m_texture_handle[app_id] = handle;
    } else {
        m_has_boxart[app_id] = false;
    }
}

int BoxArtManager::texture_id(int app_id) {
    if (m_texture_handle.count(app_id)) {
        return m_texture_handle[app_id];
    }
    return -1;
}
