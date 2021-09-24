//
//  main_tabs_view.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "main_tabs_view.hpp"
#include "favorite_tab.hpp"
#include "host_tab.hpp"
#include "add_host_tab.hpp"
#include "settings_tab.hpp"
#include "Settings.hpp"

MainTabs::MainTabs()
{
    MainTabs::instanse = this;
    refillTabs();
    lastFavoritesTabHidden = !Settings::instance().has_any_favorite();
}

void MainTabs::willAppear(bool resetState)
{
    Box::willAppear(resetState);
    updateFavorites();
}

void MainTabs::updateFavorites()
{
    bool favTabsHidden = !Settings::instance().has_any_favorite();
    if (lastFavoritesTabHidden != favTabsHidden) {
        lastFavoritesTabHidden = favTabsHidden;
        refillTabs();
    }
}

void MainTabs::refillTabs()
{
    clearTabs();

    if (Settings::instance().has_any_favorite()) {
        addTab("Favorites", FavoriteTab::create);
        addSeparator();
    }
    
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

View* MainTabs::create()
{
    return new MainTabs();
}
