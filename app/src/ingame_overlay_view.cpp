//
//  ingame_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#include "ingame_overlay_view.hpp"

IngameOverlay::IngameOverlay(StreamingView* streamView) :
    streamView(streamView)
{
    this->inflateFromXMLRes("xml/views/ingame_overlay.xml");
    
    terminate->setText("Disconnect");
    terminate->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView]{
            streamView->terminate();
        });
        return true;
    });
}

brls::AppletFrame* IngameOverlay::getAppletFrame()
{
    return applet;
}

