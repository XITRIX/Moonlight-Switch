//
//  ingame_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#include "ingame_overlay_view.hpp"
#include <libretro-common/retro_timers.h>

bool debug = false;

IngameOverlay::IngameOverlay(StreamingView* streamView) :
    streamView(streamView)
{
    this->inflateFromXMLRes("xml/views/ingame_overlay.xml");
    
    debugButton->init("Debug info", streamView->draw_stats, [streamView](bool value) {
        streamView->draw_stats = value;
    });
    
    terminateButton->setText("Disconnect");
    terminateButton->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] {
            streamView->terminate(false);
        });
        return true;
    });
}

brls::AppletFrame* IngameOverlay::getAppletFrame()
{
    return applet;
}

