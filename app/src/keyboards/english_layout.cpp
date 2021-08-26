//
//  english_layout.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 22.06.2021.
//

#include "keyboard_view.hpp"

using namespace brls;

KeyboardKeys ENG_BUTTONS[] =
{
    VK_KEY_Q,
    VK_KEY_W,
    VK_KEY_E,
    VK_KEY_R,
    VK_KEY_T,
    VK_KEY_Y,
    VK_KEY_U,
    VK_KEY_I,
    VK_KEY_O,
    VK_KEY_P,
    VK_KEY_A,
    VK_KEY_S,
    VK_KEY_D,
    VK_KEY_F,
    VK_KEY_G,
    VK_KEY_H,
    VK_KEY_J,
    VK_KEY_K,
    VK_KEY_L,
    VK_KEY_Z,
    VK_KEY_X,
    VK_KEY_C,
    VK_KEY_V,
    VK_KEY_B,
    VK_KEY_N,
    VK_KEY_M,
};

void KeyboardView::createEnglishLayout()
{
    clearViews();
    
    Box* firstRow = new Box(Axis::ROW);
    addView(firstRow);
    
    for (int i = 0; i < 10; i++)
    {
        ButtonView* button = new ButtonView();
        button->setKey(ENG_BUTTONS[i]);
        button->setMargins(4, 4, 4, 4);
        firstRow->addView(button);
    }
    
    Box* secondRow = new Box(Axis::ROW);
    addView(secondRow);
    
    for (int i = 10; i < 19; i++)
    {
        ButtonView* button = new ButtonView();
        button->setKey(ENG_BUTTONS[i]);
        button->setMargins(4, 4, 4, 4);
        secondRow->addView(button);
    }
    
    Box* thirdRow = new Box(Axis::ROW);
    addView(thirdRow);
    
    ButtonView* lshiftButton = new ButtonView();
    lshiftButton->setKey(VK_RSHIFT);
    lshiftButton->triggerType = true;
    lshiftButton->charLabel->setFontSize(21);
    lshiftButton->setMargins(4, 24, 4, 4);
    lshiftButton->setWidth(120);
    lshiftButton->event = [] {
        KeyboardView::shiftUpdated.fire();
    };
    thirdRow->addView(lshiftButton);
    
    for (int i = 19; i < 26; i++)
    {
        ButtonView* button = new ButtonView();
        button->setKey(ENG_BUTTONS[i]);
        button->setMargins(4, 4, 4, 4);
        thirdRow->addView(button);
    }
    
    ButtonView* deleteButton = new ButtonView();
    deleteButton->setKey(VK_BACK);
    deleteButton->charLabel->setFontSize(21);
    deleteButton->setMargins(4, 4, 4, 24);
    deleteButton->setWidth(120);
    thirdRow->addView(deleteButton);
    
    Box* fourthRow = new Box(Axis::ROW);
    addView(fourthRow);
    
    ButtonView* altButton = new ButtonView();
    altButton->charLabel->setText("123");
    altButton->charLabel->setFontSize(21);
    altButton->setMargins(4, 4, 4, 4);
    altButton->setWidth(120);
    altButton->event = [this] {
        createNumpadLayout();
        Application::giveFocus(this);
    };
    fourthRow->addView(altButton);
    
    ButtonView* winButton = new ButtonView();
    winButton->setKey(VK_LWIN);
    winButton->charLabel->setFontSize(21);
    winButton->setMargins(4, 4, 4, 4);
    winButton->setWidth(120);
    fourthRow->addView(winButton);
    
    ButtonView* spaceButton = new ButtonView();
    spaceButton->setKey(VK_SPACE);
    spaceButton->charLabel->setFontSize(21);
    spaceButton->setMargins(4, 4, 4, 4);
    spaceButton->setWidth(464);
    fourthRow->addView(spaceButton);
    
    ButtonView* ctrlButton = new ButtonView();
    ctrlButton->setKey(VK_RCONTROL);
    ctrlButton->triggerType = true;
    ctrlButton->charLabel->setFontSize(21);
    ctrlButton->setMargins(4, 4, 4, 4);
    ctrlButton->setWidth(120);
    fourthRow->addView(ctrlButton);
    
    ButtonView* returnButton = new ButtonView();
    returnButton->setKey(VK_RETURN);
    returnButton->charLabel->setFontSize(21);
    returnButton->setMargins(4, 4, 4, 4);
    returnButton->setWidth(120);
    fourthRow->addView(returnButton);
}
