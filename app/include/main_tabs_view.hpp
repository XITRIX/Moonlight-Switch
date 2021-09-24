//
//  main_tabs_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.05.2021.
//

#pragma once

#include <borealis.hpp>
#include <Singleton.hpp>

using namespace brls;

class MainTabs : public brls::TabFrame
{
public:
    MainTabs();
    void refillTabs();
    static View* create();

    void willAppear(bool resetState = false) override;
    
    static MainTabs* getInstanse()
    {
        return instanse;
    }
    
private:
    bool lastFavoritesTabHidden = true;
    inline static MainTabs* instanse;

    void updateFavorites();
};
