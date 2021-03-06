//
//  settings_tab.hpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#pragma once

#include <borealis.hpp>

class SettingsTab : public brls::Box
{
  public:
    SettingsTab();
    ~SettingsTab();

    BRLS_BIND(brls::SelectorCell, resolution, "resolution");
    BRLS_BIND(brls::SelectorCell, fps, "fps");
    BRLS_BIND(brls::SelectorCell, codec, "codec");
    BRLS_BIND(brls::SelectorCell, decoder, "decoder");
    BRLS_BIND(brls::Header, header, "header");
    BRLS_BIND(brls::Slider, slider, "slider");
    BRLS_BIND(brls::BooleanCell, optimal, "optimal");
    BRLS_BIND(brls::BooleanCell, pcAudio, "pcAudio");
    BRLS_BIND(brls::BooleanCell, writeLog, "writeLog");
    BRLS_BIND(brls::BooleanCell, swapUi, "swap_ui");
    BRLS_BIND(brls::BooleanCell, swapGame, "swap_game");

    static brls::View* create();
};
