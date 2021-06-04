#include "Singleton.hpp"
#include <stdio.h>
#include <string>
#include <map>
#include <vector>
#pragma once

struct NVGcontext;
struct Data;

class BoxArtManager: public Singleton<BoxArtManager> {
public:
    bool has_boxart(int app_id);
    
    void set_data(Data data, int app_id);
    std::string get_texture_path(int app_id);
    void make_texture_from_boxart(NVGcontext *ctx, int app_id);
    int texture_id(int app_id);
    
private:
    std::map<int, bool> m_has_boxart;
    std::map<int, int> m_texture_handle;
};
