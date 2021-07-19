//
//  button_selecting_dialog.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 19.07.2021.
//

#include "button_selecting_dialog.hpp"
#include "Settings.hpp"

ButtonSelectingDialog::ButtonSelectingDialog(Box* box):
    Dialog(box) {}

ButtonSelectingDialog::~ButtonSelectingDialog()
{
    Application::unblockInputs();
}

ButtonSelectingDialog* ButtonSelectingDialog::create(std::string titleText) {
    brls::Style style = brls::Application::getStyle();

    brls::Label* label = new brls::Label();
    label->setFontSize(style["brls/dialog/fontSize"]);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);

    brls::Box* box = new brls::Box();
    box->addView(label);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(style["brls/dialog/paddingTopBottom"], style["brls/dialog/paddingLeftRight"], style["brls/dialog/paddingTopBottom"], style["brls/dialog/paddingLeftRight"]);
    
    ButtonSelectingDialog* dialog = new ButtonSelectingDialog(box);
    dialog->titleText = titleText;
    dialog->label = label;
    dialog->setCancelable(false);
    dialog->reloadLabel();
    
    dialog->button1->setHideHighlight(true);
    dialog->button2->setHideHighlight(true);
    dialog->button3->setHideHighlight(true);
    
    return dialog;
}

void ButtonSelectingDialog::open()
{
    Application::getPlatform()->getInputManager()->updateControllerState(&oldState);
    Dialog::open();
    Application::blockInputs();
}

void ButtonSelectingDialog::resetButtons()
{
    buttons.clear();
    reloadLabel();
}

void ButtonSelectingDialog::reloadLabel()
{
    label->setText(titleText + buttonsText());
}

std::string ButtonSelectingDialog::buttonsText()
{
    std::string buttonsText = "";
    for (int i = 0; i < buttons.size(); i++) {
        buttonsText += brls::Hint::getKeyIcon(buttons[i]);
        if (i < buttons.size() - 1)
            buttonsText += " + ";
    }
    return buttonsText;
}

void ButtonSelectingDialog::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    Dialog::draw(vg, x, y, width, height, style, ctx);
    
    ControllerState state;
    Application::getPlatform()->getInputManager()->updateControllerState(&state);
    
    for (int i = 0; i < ControllerButton::_BUTTON_MAX; i++)
    {
        ControllerButton button = (ControllerButton) i;
        if (state.buttons[i] && !oldState.buttons[i])
            if(std::find(buttons.begin(), buttons.end(), button) == buttons.end())
            {
                buttons.push_back(button);
                reloadLabel();
            }
    }
    
    oldState = state;
}
