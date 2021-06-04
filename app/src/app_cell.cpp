//
//  app_cell.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 03.06.2021.
//

#include "app_cell.hpp"
#include "streaming_view.hpp"
#include "BoxArtManager.hpp"

AppCell::AppCell(Host host, AppInfo app)
{
    this->inflateFromXMLRes("xml/cells/app_cell.xml");
    
    title->setText(app.name);
    title->setTextColor(nvgRGB(255,255,255));
    
    this->registerClickAction([this, host, app](View* view)
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
