#pragma once

#include <cmath>
#include <iomanip>
#include <sstream>

#include "Settings.hpp"

inline void refreshMouseScrollSettings(brls::Slider* slider)
{
    slider->setProgress((Settings::instance().get_mouse_scroll_multiplier() - 0.1f) / 1.9f);
}

inline void setupMouseScrollSettings(brls::Header* header, brls::Slider* slider)
{
    slider->getProgressEvent()->subscribe(
        [header](float value)
        {
            const int percent = static_cast<int>(std::lround((0.1f + value * 1.9f) * 100.0f));
            Settings::instance().set_mouse_scroll_multiplier(percent);
            std::stringstream stream;
            stream << std::fixed << std::setprecision(2) << Settings::instance().get_mouse_scroll_multiplier();
            header->setSubtitle("x" + stream.str());
        });
    refreshMouseScrollSettings(slider);
}
