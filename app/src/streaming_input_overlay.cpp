//
//  streaming_input_overlay.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 25.08.2021.
//

#include "streaming_input_overlay.hpp"

using namespace brls;

StreamingInputOverlay::StreamingInputOverlay(StreamingView* streamView) :
    streamView(streamView)
{
    this->inflateFromXMLRes("xml/views/stream_input_overlay.xml");
//    setFocusable(true);
    
    inner->setHideHighlightBackground(true);
    inner->setHideHighlightBorder(true);
    
//    hintBar->setAlpha(0.3f);
    NVGcolor color = Application::getTheme()["brls/background"];
    color.a = 0.3f;
    hintBar->setBackgroundColor(color);
    
    addGestureRecognizer(new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
        if (status.state == GestureState::END)
            this->dismiss();
    }));
    
    applet->addGestureRecognizer(new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) { }));
    
    registerAction("Mouse", ControllerButton::BUTTON_LSB, [](View* view){return true;});
    registerAction("Scroll", ControllerButton::BUTTON_RSB, [](View* view){return true;});
    registerAction("Keyboard", ControllerButton::BUTTON_X, [this](View* view) {
        this->toggleKeyboard();
        return true;
    });
//    registerAction("Back", ControllerButton::BUTTON_LSB, [](View* view){return true;});
    
    keyboard = new KeyboardView();
    keyboard->setVisibility(isKeyboardOpen ? Visibility::VISIBLE : Visibility::GONE);
    inner->addView(keyboard, 0);
}

void StreamingInputOverlay::toggleKeyboard()
{
    isKeyboardOpen = !isKeyboardOpen;
    keyboard->setVisibility(isKeyboardOpen ? Visibility::VISIBLE : Visibility::GONE);
}


brls::AppletFrame* StreamingInputOverlay::getAppletFrame()
{
    return applet;
}
