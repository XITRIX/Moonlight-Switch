/*
    Copyright 2021 natinusala

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include <borealis.hpp>

class SettingsTab : public brls::Box
{
  public:
    SettingsTab();

    BRLS_BIND(brls::SelectorCell, resolution, "resolution");
    BRLS_BIND(brls::SelectorCell, fps, "fps");
    BRLS_BIND(brls::SelectorCell, codec, "codec");
    BRLS_BIND(brls::SelectorCell, decoder, "decoder");
    BRLS_BIND(brls::Header, header, "header");
    BRLS_BIND(brls::Slider, slider, "slider");
    BRLS_BIND(brls::BooleanCell, optimal, "optimal");
    BRLS_BIND(brls::BooleanCell, pcAudio, "pcAudio");
    BRLS_BIND(brls::BooleanCell, writeLog, "writeLog");

    static brls::View* create();
};
