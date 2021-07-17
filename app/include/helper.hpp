//
//  helper.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#pragma once

#include <borealis.hpp>

void showAlert(std::string message, std::function<void(void)> cb = []{});
void showError(std::string message, std::function<void(void)> cb = []{});
brls::Dialog* createLoadingDialog(std::string text);
