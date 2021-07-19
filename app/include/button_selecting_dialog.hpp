//
//  button_selecting_dialog.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 19.07.2021.
//

#pragma once

#include <borealis.hpp>

using namespace brls;

class ButtonSelectingDialog: public Dialog {
public:
    ~ButtonSelectingDialog();
    static ButtonSelectingDialog* create(std::string titleText);
    void resetButtons();
    
    void open() override;
    
    std::vector<ControllerButton> getButtons() const
    {
        return buttons;
    }
    
private:
    std::string titleText;
    std::vector<ControllerButton> buttons;
    ButtonSelectingDialog(Box* box);
    ControllerState oldState;
    Label* label;
    
    void reloadLabel();
    std::string buttonsText();
    
    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) override;
};
