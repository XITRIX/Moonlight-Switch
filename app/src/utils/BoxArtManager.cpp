#include "BoxArtManager.hpp"
#include "Settings.hpp"
#include "Data.hpp"
#include "nanovg.h"
#include <algorithm>
#include <mutex>

std::mutex m_mutex;

bool BoxArtManager::has_boxart(int app_id) {
    if (m_has_boxart.count(app_id)) {
        return m_has_boxart[app_id];
    }
    
    std::string path = Settings::instance().boxart_dir() + "/" + std::to_string(app_id) + ".png";
    Data data = Data::read_from_file(path);
    m_has_boxart[app_id] = !data.is_empty();
    
    return m_has_boxart[app_id];
}

void BoxArtManager::set_data(Data data, int app_id) {
    std::lock_guard<std::mutex> guard(m_mutex);
    
    std::string path = Settings::instance().boxart_dir() + "/" + std::to_string(app_id) + ".png";
    data.write_to_file(path);
    m_has_boxart[app_id] = true;
}

void BoxArtManager::make_texture_from_boxart(NVGcontext *ctx, int app_id) {
    std::lock_guard<std::mutex> guard(m_mutex);
    
    std::string path = Settings::instance().boxart_dir() + "/" + std::to_string(app_id) + ".png";
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
