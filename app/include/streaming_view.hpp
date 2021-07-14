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

typedef brls::Event<> FingersGestureEvent;
class FingersGestureRecognizer: public brls::GestureRecognizer
{
public:
    FingersGestureRecognizer(int fingers, FingersGestureEvent::Callback respond);
    brls::GestureState recognitionLoop(std::array<brls::TouchState, TOUCHES_MAX> touches,brls:: MouseState mouse, brls::View* view, brls::Sound* soundToPlay) override;
private:
    int fingers;
    FingersGestureEvent event;
};

class StreamingView : public brls::Box
{
public:
    StreamingView(Host host, AppInfo app);
    ~StreamingView();
    
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
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
    Box* keyboardHolder = nullptr;
    KeyboardView* keyboard;
    bool blocked = false;
    
    void handleInput();
    void handleButtonHolding();
};
