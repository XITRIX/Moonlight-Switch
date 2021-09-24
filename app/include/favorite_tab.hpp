//
//  favorite_tab.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.09.2021.
//

#pragma once

#include <borealis.hpp>
#include <Settings.hpp>

class FavoriteTab : public brls::Box
{
  public:
    FavoriteTab();

    static brls::View* create();
};
