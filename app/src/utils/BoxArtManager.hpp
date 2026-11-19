#include "Singleton.hpp"
#include <map>
#include <cstdio>
#include <string>
#include <vector>
#pragma once

struct NVGcontext;
struct Data;
struct Host;

class BoxArtManager : public Singleton<BoxArtManager> {
  public:
    BoxArtManager();

    bool has_boxart(const Host& host, int app_id);

    void set_data(Data data, const Host& host, int app_id);
    static std::string get_texture_path(const Host& host, int app_id);
    void make_texture_from_boxart(NVGcontext* ctx, const Host& host,
                                  int app_id);
    int texture_id(const Host& host, int app_id);
    void remove_host(const Host& host);

  private:
    std::map<std::string, bool> m_has_boxart;
    std::map<std::string, int> m_texture_handle;
    static void compress_texture(const std::string& path);
};
