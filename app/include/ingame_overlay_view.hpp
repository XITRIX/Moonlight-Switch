//
//  ingame_overlay.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#pragma once

#include "streaming_view.hpp"
#include <borealis.hpp>

class IngameOverlay : public brls::Box
{
public:
    IngameOverlay(StreamingView* streamView);
    
    void show();
    
    brls::AppletFrame* getAppletFrame() override;

    
    bool isTranslucent() override
    {
        return true;
    }
    
private:
    StreamingView* streamView;
    
    BRLS_BIND(brls::BooleanCell, debugButton, "debug");
    BRLS_BIND(brls::DetailCell, terminateButton, "terminate");
    BRLS_BIND(brls::AppletFrame, applet, "applet");
};
