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

using namespace brls;

class StreamingView : public View
{
public:
    StreamingView(Host host, AppInfo app);
    ~StreamingView();
    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) override;
    void onFocusLost() override;
    
private:
    bool draw_stats = false;
    Host host;
    AppInfo app;
    MoonlightSession* session;
    
    void handleInput();
    void terminate();
};
