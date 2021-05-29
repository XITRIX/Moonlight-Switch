//
//  helper.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#pragma once

#include <borealis.hpp>

void showError(brls::View* presenter, std::string message, std::function<void(void)> cb);
