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
    
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    brls::AppletFrame* getAppletFrame() override;
    void onFocusGained() override;
    bool isTranslucent() override { return true; }
private:
    StreamingView* streamView;
    KeyboardView* keyboard;
    bool isKeyboardOpen = false;
    
    void toggleKeyboard();
    
    BRLS_BIND(brls::Box, inner, "inner");
    BRLS_BIND(brls::Box, hintBar, "hint_bar");
    BRLS_BIND(brls::AppletFrame, applet, "applet");
    
    std::vector<brls::ActionIdentifier> actionsToFree;
};

