//
//  favorite_tab.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.09.2021.
//

#include "favorite_tab.hpp"

using namespace brls::literals;

FavoriteTab::FavoriteTab()
{

}

brls::View* FavoriteTab::create()
{
    return new FavoriteTab();
}
