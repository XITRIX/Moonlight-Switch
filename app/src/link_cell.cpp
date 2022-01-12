//
//  link_cell.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 21.11.2021.
//

#include "link_cell.hpp"

using namespace brls;

LinkCell::LinkCell() {
    this->inflateFromXMLRes("xml/cells/link_cell.xml");

    icon->setText("\uE099");
}

View* LinkCell::create() { return new LinkCell(); }
