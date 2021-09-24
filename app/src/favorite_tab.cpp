//
//  favorite_tab.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.09.2021.
//

#include "favorite_tab.hpp"
#include "app_cell.hpp"

using namespace brls;
using namespace brls::literals;

FavoriteTab::FavoriteTab()
{
    this->inflateFromXMLRes("xml/tabs/favorites.xml");

    container->setHideHighlight(true);

    updateAppList();
}

void FavoriteTab::updateAppList()
{
    std::vector<Host> hosts = Settings::instance().hosts();
    for (Host host : hosts) {
        Header* header = new Header();
        header->setTitle(host.hostname);
        header->setMarginLeft(60);
        header->setMarginBottom(20);
        container->addView(header);

        GridView* gridView = new GridView(5);

        for (App app : host.favorites) {
            AppInfo info {app.name, app.app_id};
            AppCell* cell = new AppCell(host, info, false);
            gridView->addView(cell);
        }

        container->addView(gridView);
    }
}

brls::View* FavoriteTab::create()
{
    return new FavoriteTab();
}
