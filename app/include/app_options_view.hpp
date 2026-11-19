//
//  app_options_view.hpp
//  Moonlight
//

#pragma once

#include "GameStreamClient.hpp"
#include <Settings.hpp>
#include <borealis.hpp>
#include <functional>

class AppOptionsView : public brls::Box {
  public:
    AppOptionsView(const Host& host, const AppInfo& app);

  private:
    Host host;
    int appId;
    AppStreamOptions options;
    std::vector<brls::SelectorCell*> cells;

    void addRow(const std::string& title, const std::string& global,
                std::vector<std::string> names, int selected,
                std::function<void(int)> apply);
    void reset();
};
