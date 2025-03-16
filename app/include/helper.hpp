//
//  helper.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#pragma once

#include <borealis.hpp>

void showAlert(
    std::string message, const std::function<void(void)>& cb = [] {});
void showError(
    const std::string& message, const std::function<void(void)>& cb = [] {});
brls::Dialog* createLoadingDialog(const std::string& text);
