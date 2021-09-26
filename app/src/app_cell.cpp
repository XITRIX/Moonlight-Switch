//
//  app_cell.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.06.2021.
//

#include "app_cell.hpp"
#include "streaming_view.hpp"
#include "BoxArtManager.hpp"
#include "Settings.hpp"
#include "main_tabs_view.hpp"

AppCell::AppCell(Host host, AppInfo app, int currentApp)
{
    this->inflateFromXMLRes("xml/cells/app_cell.xml");
    this->setFavorite(false);

    title->setText(app.name);
    title->setTextColor(nvgRGB(255,255,255));
    
    currentAppImage->setVisibility(currentApp == app.app_id ? Visibility::VISIBLE : Visibility::GONE);
    
    this->addGestureRecognizer(new TapGestureRecognizer(this));
    this->registerClickAction([host, app](View* view)
    {
        AppletFrame* frame = new AppletFrame(new StreamingView(host, app));
        frame->setHeaderVisibility(brls::Visibility::GONE);
        frame->setFooterVisibility(brls::Visibility::GONE);
        Application::pushActivity(new Activity(frame));
        return true;
    });
    
    if (BoxArtManager::instance().has_boxart(app.app_id))
        image->setImageFromFile(BoxArtManager::instance().get_texture_path(app.app_id));
    else
    {
        ASYNC_RETAIN
        GameStreamClient::instance().app_boxart(host.address, app.app_id, [ASYNC_TOKEN, host, app](auto result) {
            ASYNC_RELEASE
            
            if (result.isSuccess()) {
                BoxArtManager::instance().set_data(result.value(), app.app_id);
                image->setImageFromFile(BoxArtManager::instance().get_texture_path(app.app_id));
            }
        });
    }
}

void AppCell::setFavorite(bool favorite)
{
    favoriteAppImage->setVisibility(favorite ? Visibility::VISIBLE : Visibility::GONE);
}
