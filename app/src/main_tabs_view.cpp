//
//  main_tabs_view.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "main_tabs_view.hpp"
#include "host_tab.hpp"
#include "add_host_tab.hpp"
#include "settings_tab.hpp"
#include "about_tab.hpp"
#include "Settings.hpp"

MainTabs::MainTabs()
{
    favoriteTab = new FavoriteTab();
    favoriteTab->ptrLock();
    
    MainTabs::instanse = this;
    refillTabs();
    lastHasAnyFavorites = Settings::instance().has_any_favorite();
}

void MainTabs::willAppear(bool resetState)
{
    Box::willAppear(resetState);
    updateFavoritesIfNeeded();
    favoriteTab->refreshIfNeeded();
}

void MainTabs::updateFavoritesIfNeeded()
{
    if (lastHasAnyFavorites != Settings::instance().has_any_favorite()) {
        refillTabs();
    }
}

void MainTabs::refillTabs()
{
    clearTabs();

    bool hasAnyFavorite = Settings::instance().has_any_favorite();
    if (hasAnyFavorite) {
        addTab("tabs/favorites"_i18n, [this] { return this->favoriteTab; });
        addSeparator();
    }
    lastHasAnyFavorites = hasAnyFavorite;
    
    auto hosts = Settings::instance().hosts();
    for (Host host : hosts)
    {
        addTab(host.hostname, [host]{ return new HostTab(host); });
    }
    if (hosts.size() > 0)
        addSeparator();
    
    addTab("tabs/add_host"_i18n, AddHostTab::create);
    addTab("tabs/settings"_i18n, SettingsTab::create);
    addSeparator();
    addTab("tabs/about"_i18n, AboutTab::create);
    focusTab(0);
}

View* MainTabs::create()
{
    return new MainTabs();
}
