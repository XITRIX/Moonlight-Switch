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
    AppCell(Host host, AppInfo app);
    
//    Image* image;
//    Label* title;
    BRLS_BIND(Image, image, "image");
    BRLS_BIND(Label, title, "title");
};

