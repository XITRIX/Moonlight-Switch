//
//  hosts_tabs_view.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "hosts_tabs_view.hpp"
#include "host_tab.hpp"
#include "add_host_tab.hpp"
#include "settings_tab.hpp"
#include "Settings.hpp"

HostsTabs::HostsTabs()
{
    HostsTabs::instanse = this;
    refillTabs();
}

void HostsTabs::refillTabs()
{
    clearTabs();
    
    
    auto hosts = Settings::instance().hosts();
    for (Host host : hosts)
    {
        addTab(host.hostname, [host]{ return new HostTab(host); });
    }
    if (hosts.size() > 0)
        addSeparator();
    
    addTab("tabs/add_host"_i18n, AddHostTab::create);
    addTab("tabs/settings"_i18n, SettingsTab::create);
    focusTab(0);
}

View* HostsTabs::create()
{
    return new HostsTabs();
}
