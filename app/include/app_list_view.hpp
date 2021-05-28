//
//  app_list_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 26.05.2021.
//

#pragma once

#include <borealis.hpp>
#include <Settings.hpp>

using namespace brls;

class AppListView : public Box
{
public:
    AppListView(Host host);
    
private:
    Host host;
    
    BRLS_BIND(Box, container, "container");
    
    void updateAppList();
};
