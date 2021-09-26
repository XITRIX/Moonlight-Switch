//
//  app_cell.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.06.2021.
//

#pragma once

#include "GameStreamClient.hpp"
#include <borealis.hpp>

using namespace brls;

class AppCell : public Box
{
public:
    AppCell(Host host, AppInfo app, int currentApp);
    
    BRLS_BIND(Image, image, "image");
    BRLS_BIND(Label, title, "title");
    BRLS_BIND(Image, currentAppImage, "current_app_image");
    BRLS_BIND(Image, favoriteAppImage, "favorite_app_image");

    void setFavorite(bool favorite);

private:
    void updateFavoriteAction(Host host, AppInfo app);
};

