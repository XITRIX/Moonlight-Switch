//
//  main_tabs_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.05.2021.
//

#pragma once

#include "favorite_tab.hpp"
#include <Singleton.hpp>
#include <borealis.hpp>

using namespace brls;

class MainTabs : public brls::TabFrame {
  public:
    MainTabs();
    void refillTabs(bool keepFocus = true);
    static View* create();

    void willAppear(bool resetState) override;
    FavoriteTab* getFavoriteTab() { return favoriteTab; }
    void updateFavoritesIfNeeded();

    static MainTabs* getInstanse() { return instanse; }

  private:
    bool lastHasAnyFavorites = false;
    inline static MainTabs* instanse;

    FavoriteTab* favoriteTab;
};
