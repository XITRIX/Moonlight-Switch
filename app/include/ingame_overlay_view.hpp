//
//  ingame_overlay.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#pragma once

#include "streaming_view.hpp"
#include <borealis.hpp>

// MARK: - Ingame Overlay View
class IngameOverlay : public brls::Box
{
public:
    IngameOverlay(StreamingView* streamView);
    
    void show();
    
    brls::AppletFrame* getAppletFrame() override;

    bool isTranslucent() override { return true; }
    
private:
    StreamingView* streamView;
    
    BRLS_BIND(brls::Box, backplate, "backplate");
    BRLS_BIND(brls::AppletFrame, applet, "applet");
};

// MARK: - Logout Tab
class LogoutTab : public brls::Box
{
public:
    LogoutTab(StreamingView* streamView);
private:
    StreamingView* streamView;
    
    BRLS_BIND(brls::DetailCell, disconnect, "disconnect");
    BRLS_BIND(brls::DetailCell, terminateButton, "terminate");
};

// MARK: - Debug Tab
class DebugTab : public brls::Box
{
public:
    DebugTab(StreamingView* streamView);
private:
    StreamingView* streamView;
    
    BRLS_BIND(brls::BooleanCell, debugButton, "debug");
    BRLS_BIND(brls::BooleanCell, onscreenLogButton, "onscreen_log");
};

// MARK: - Keys Tab
class KeysTab : public brls::Box
{
public:
    KeysTab(StreamingView* streamView);
private:
    StreamingView* streamView;
    
    BRLS_BIND(brls::DetailCell, escButton, "esc_button");
};

