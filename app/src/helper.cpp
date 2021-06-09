//
//  helper.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#include "helper.hpp"

void showError(std::string message, std::function<void(void)> cb)
{
    auto alert = new brls::Dialog(message);
    alert->addButton("Close", [alert, cb]
    {
        cb();
    });
    alert->setCancelable(false);
    alert->open();
}
