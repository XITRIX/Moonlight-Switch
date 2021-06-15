//
//  streaming_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#pragma once

#include <borealis.hpp>
#include <Settings.hpp>
#include "GameStreamClient.hpp"
#include "MoonlightSession.hpp"
#include "loading_overlay.hpp"
#include "keyboard_view.hpp"
#include <optional>

using namespace brls;

typedef brls::Event<> FingersGestureEvent;
class FingersGestureRecognizer: public GestureRecognizer
{
public:
    FingersGestureRecognizer(int fingers, FingersGestureEvent::Callback respond);
    GestureState recognitionLoop(std::array<TouchState, TOUCHES_MAX> touches, MouseState mouse, View* view, Sound* soundToPlay) override;
private:
    int fingers;
    FingersGestureEvent event;
};

class StreamingView : public Box
{
public:
    StreamingView(Host host, AppInfo app);
    ~StreamingView();
    
    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) override;
    void onFocusGained() override;
    void onFocusLost() override;
    void onLayout() override;
    
    void terminate(bool terminateApp);
    
    bool draw_stats = false;
private:
    Host host;
    AppInfo app;
    MoonlightSession* session;
    LoadingOverlay* loader = nullptr;
    std::optional<PanGestureStatus> panStatus;
    KeyboardView* keyboard;
    
    void handleInput();
    void handleButtonHolding();
};
