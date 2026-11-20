#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

inline std::vector<std::string> systemButtonOptions()
{
    using namespace brls::literals;
    std::vector<std::string> options
        = { "hints/off"_i18n, "settings/buttons/screenshot"_i18n, "settings/buttons/home"_i18n };
#ifdef __SWITCH2__
    options.push_back("settings/buttons/chat"_i18n);
#endif
    return options;
}
