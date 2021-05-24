//
//  hosts_tabs_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.05.2021.
//

#include "hosts_tabs_view.hpp"
#include "host_tab.hpp"
#include "add_host_tab.hpp"
#include "settings_tab.hpp"

HostsTabs::HostsTabs()
{
    refillTabs();
}

void HostsTabs::refillTabs()
{
    clearTabs();
    
    addTab("XITRIX", HostTab::create);
    addSeparator();
    
    addTab("Add host", ComponentsTab::create);
    addTab("Settings", SettingsTab::create);
}

View* HostsTabs::create()
{
    return new HostsTabs();
}
