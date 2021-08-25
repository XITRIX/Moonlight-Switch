//
//  streaming_input_overlay.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 25.08.2021.
//

#pragma once

#include "streaming_view.hpp"
#include "keyboard_view.hpp"
#include <borealis.hpp>

class StreamingInputOverlay: public brls::Box
{
public:
    StreamingInputOverlay(StreamingView* streamView);
    
    void show();
    
    brls::AppletFrame* getAppletFrame() override;
    bool isTranslucent() override { return true; }
private:
    StreamingView* streamView;
    KeyboardView* keyboard;
    bool isKeyboardOpen = false;
    
    void toggleKeyboard();
    
    BRLS_BIND(brls::Box, inner, "inner");
    BRLS_BIND(brls::Box, hintBar, "hint_bar");
    BRLS_BIND(brls::AppletFrame, applet, "applet");
    
    
};

