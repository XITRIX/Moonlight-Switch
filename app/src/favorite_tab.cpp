//
//  favorite_tab.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 24.09.2021.
//

#include "favorite_tab.hpp"
#include "app_cell.hpp"
#include "main_tabs_view.hpp"
#include "forwarder_maker.hpp"

using namespace brls;
using namespace brls::literals;

FavoriteTab::FavoriteTab() {
    this->inflateFromXMLRes("xml/tabs/favorites.xml");

    container->setHideHighlight(true);

    updateAppList();
}

void FavoriteTab::refreshIfNeeded() {
    if (isDirty) {
        isDirty = false;
        updateAppList();
    }
}

void FavoriteTab::updateAppList() {
    container->clearViews();
    std::vector<Host> hosts = Settings::instance().hosts();
    for (const Host& host : hosts) {
        if (host.favorites.empty())
            continue;

        auto* header = new Header();
        header->setTitle(host.hostname);
        header->setLineBottom(0);

        if (!container->getChildren().empty())
            header->setMarginTop(40);

        container->addView(header);

        auto* gridView = new GridView(5);

        for (const App& app : host.favorites) {
            AppInfo info{app.name, app.app_id};
            auto* cell = new AppCell(host, info, 0);
            gridView->addView(cell);

            cell->registerAction(
                "Make forwarder", BUTTON_X,
                [host, app](View* view) {
                    makeForwarder(host, app);
                    return true;
            });
            cell->registerAction(
                "app_list/unstar"_i18n, BUTTON_Y,
                [this, gridView, cell, host, app](View* view) {
                    int index = gridView->getItemIndex(cell);
                    int section = *((int*)gridView->getParentUserData());

                    Settings::instance().remove_favorite(host, app.app_id);
                    this->updateAppList();

                    if (section >= container->getChildren().size())
                        section = (int)container->getChildren().size() - 2;

                    if (section <= 0) {
                        MainTabs::getInstanse()->updateFavoritesIfNeeded();
                        return true;
                    }

                    int newIndex = index;
                    int items =
                        (int)((GridView*)container->getChildren()[section])
                            ->getChildren()
                            .size();
                    if (newIndex >= items)
                        newIndex = items - 1;

                    if (newIndex < 0)
                        return true;

                    View* focus = ((GridView*)container->getChildren()[section])
                                      ->getChildren()[newIndex];
                    Application::giveFocus(focus);

                    return true;
                });
        }

        container->addView(gridView);
    }
}

brls::View* FavoriteTab::create() { return new FavoriteTab(); }
