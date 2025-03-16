//
//  helper.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 29.05.2021.
//

#include "helper.hpp"

#include <utility>

using namespace brls;

void showAlert(std::string message, const std::function<void(void)>& cb) {
    auto alert = new brls::Dialog(std::move(message));
    alert->addButton("common/close"_i18n, [cb] { cb(); });
    alert->setCancelable(false);
    alert->open();
}

void showError(const std::string& message, const std::function<void(void)>& cb) {
    showAlert("error/dialog_header"_i18n + "\n\n" + message, cb);
}

brls::Dialog* createLoadingDialog(const std::string& text) {
    Style style = Application::getStyle();
    Box* holder = new Box(Axis::COLUMN);

    auto* label = new Label();
    label->setText(text);
    label->setFontSize(style["brls/dialog/fontSize"]);
    label->setHorizontalAlign(HorizontalAlign::CENTER);
    label->setMarginBottom(21);

    auto* spinner = new ProgressSpinner(ProgressSpinnerSize::LARGE);
    spinner->View::setSize(Size(92, 92));

    holder->addView(label);
    holder->addView(spinner);

    holder->setAlignItems(AlignItems::CENTER);
    holder->setJustifyContent(JustifyContent::CENTER);
    holder->setPadding(style["brls/dialog/paddingTopBottom"],
                       style["brls/dialog/paddingLeftRight"], 28,
                       style["brls/dialog/paddingLeftRight"]);

    auto* dialog = new Dialog(holder);
    dialog->setCancelable(false);
    dialog->setFocusable(true);
    dialog->setHideHighlight(true);
    return dialog;
}
