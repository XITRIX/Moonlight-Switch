//
//  app_list_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 26.05.2021.
//

#pragma once

#include <borealis.hpp>
#include <Settings.hpp>
#include "GameStreamClient.hpp"
#include "loading_overlay.hpp"
#include "grid_view.hpp"
#include "app_cell.hpp"

#include <optional>

using namespace brls;

class AppListView : public Box
{
public:
    AppListView(Host host);
    
    void onLayout() override;
    void willAppear(bool resetState) override;
    View* getHintView() override;
    
private:
    Host host;
    View* hintView = nullptr;
    std::optional<AppInfo> currentApp;
    bool loading = false;
    bool inputBlocked = false;
    LoadingOverlay* loader = nullptr;
    ActionIdentifier terminateIdentifier;
    void blockInput(bool block);
    
    GridView* gridView;
    BRLS_BIND(Box, container, "container");
    
    void setCurrentApp(AppInfo app, bool update = false);
    void terninateApp();
    void updateAppList();
    void updateFavoriteAction(AppCell* cell, Host host, AppInfo app);
};
