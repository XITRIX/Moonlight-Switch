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
    static ButtonSelectingDialog* create(std::string titleText, std::function<void(std::vector<ControllerButton>)> callback);
    void open() override;
    
private:
    std::function<void(std::vector<ControllerButton>)> callback;
    std::string titleText;
    std::vector<ControllerButton> buttons;
    ControllerState oldState;
    Label* label;
    
    ButtonSelectingDialog(Box* box, std::function<void(std::vector<ControllerButton>)> callback);
    
    void reloadLabel();
    std::string buttonsText();
    
    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) override;
};
