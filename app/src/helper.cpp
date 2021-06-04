//
//  helper.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#include "helper.hpp"

void showError(brls::View* presenter, std::string message, std::function<void(void)> cb)
{
    auto alert = new brls::Dialog(message);
    alert->addButton("Close", [presenter, cb](brls::View* view)
    {
        view->dismiss([cb]() {
            cb();
        });
    });
    alert->setCancelable(false);
    alert->open();
}
