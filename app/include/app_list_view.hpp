//
//  app_list_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 26.05.2021.
//

#pragma once

#include <borealis.hpp>
#include <Settings.hpp>
#include "loading_overlay.hpp"
#include "grid_view.hpp"

using namespace brls;

class AppListView : public Box
{
public:
    AppListView(Host host);
    
    void onLayout() override;
    
private:
    Host host;
    LoadingOverlay* loader = nullptr;
    
    GridView* gridView;
    BRLS_BIND(Box, container, "container");
    
    void updateAppList();
};
