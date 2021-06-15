//
//  keyboard_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 14.06.2021.
//

#include "keyboard_view.hpp"

short KeyboardCodes[_VK_KEY_MAX]
{
    0x08,
    0x1B,
    0x30,
    0x31,
    0x32,
    0x33,
    0x34,
    0x35,
    0x36,
    0x37,
    0x38,
    0x39,
    0x41,
    0x42,
    0x43,
    0x44,
    0x45,
    0x46,
    0x47,
    0x48,
    0x49,
    0x4A,
    0x4B,
    0x4C,
    0x4D,
    0x4E,
    0x4F,
    0x50,
    0x51,
    0x52,
    0x53,
    0x54,
    0x55,
    0x56,
    0x57,
    0x58,
    0x59,
    0x5A,
    0x0D,
    0x20,
    0xA2,
    0xA4,
    0xA0,
    0x5B,
};

std::string KeyboardLocalization[_VK_KEY_MAX]
{
    "Delete",
    "Esc",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "Return",
    "Space",
    "Ctrl",
    "Alt",
    "Shift",
    "Win",
};

KeyboardKeys BUTTONS[] =
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

bool keysState[_VK_KEY_MAX];

ButtonView::ButtonView()
{
    setBackgroundColor(RGB(60, 60, 60));
    setWidth(90);
    setHeight(56);
    
    setAlignItems(AlignItems::CENTER);
    setJustifyContent(JustifyContent::CENTER);
    
    charLabel = new Label();
    charLabel->setHorizontalAlign(HorizontalAlign::CENTER);
    charLabel->setVerticalAlign(VerticalAlign::CENTER);
    charLabel->setTextColor(RGB(255, 255, 255));
    charLabel->setFontSize(27);
    addView(charLabel);
    
    setCornerRadius(8);
    setShadowVisibility(true);
    setShadowType(ShadowType::GENERIC);
    
    registerCallback();
}

void ButtonView::setKey(KeyboardKeys key)
{
    charLabel->setText(KeyboardLocalization[key]);
    this->key = key;
}

void ButtonView::registerCallback()
{
    addGestureRecognizer(new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
        if (!triggerType)
        {
            this->playClickAnimation(status.state != GestureState::UNSURE);
            
            switch (status.state) {
                case brls::GestureState::UNSURE:
                    keysState[key] = true;
                    break;
                default:
                    keysState[key] = false;
                    break;
            }
        }
        else
        {
            switch (status.state) {
                case brls::GestureState::UNSURE:
                    if (!keysState[key])
                        this->playClickAnimation(false);
                    break;
                case brls::GestureState::END:
                    keysState[key] = !keysState[key];
                    
                    if (!keysState[key])
                        this->playClickAnimation(!keysState[key]);
                    break;
                default:
                    break;
            }
        }
    }));
}

KeyboardView::KeyboardView()
    : Box(Axis::COLUMN)
{
    for (int i = 0; i < _VK_KEY_MAX; i++)
        keysState[i] = false;
    
    setBackgroundColor(RGBA(120, 120, 120, 200));
    setAlignItems(AlignItems::CENTER);
    setPaddingTop(24);
    setPaddingBottom(24);
    
    createEnglishLayout();
    
    addGestureRecognizer(new TapGestureRecognizer([](TapGestureStatus status, Sound* sound){}));
    addGestureRecognizer(new PanGestureRecognizer([](PanGestureStatus status, Sound* sound){}, PanAxis::ANY));
}

KeyboardState KeyboardView::getKeyboardState()
{
    KeyboardState state;
    
    for (int i = 0; i < _VK_KEY_MAX; i++)
        state.keys[i] = keysState[i];
    
    return state;
}

short KeyboardView::getKeyCode(KeyboardKeys key)
{
    return KeyboardCodes[key];
}

void KeyboardView::createEnglishLayout()
{
    clearViews();
    
    Box* firstRow = new Box(Axis::ROW);
    addView(firstRow);
    
    for (int i = 0; i < 10; i++)
    {
        ButtonView* button = new ButtonView();
        button->setKey(BUTTONS[i]);
        button->setMargins(4, 4, 4, 4);
        firstRow->addView(button);
    }
    
    Box* secondRow = new Box(Axis::ROW);
    addView(secondRow);
    
    for (int i = 10; i < 19; i++)
    {
        ButtonView* button = new ButtonView();
        button->setKey(BUTTONS[i]);
        button->setMargins(4, 4, 4, 4);
        secondRow->addView(button);
    }
    
    Box* thirdRow = new Box(Axis::ROW);
    addView(thirdRow);
    
    ButtonView* lshiftButton = new ButtonView();
    lshiftButton->setKey(VK_LSHIFT);
    lshiftButton->triggerType = true;
    lshiftButton->charLabel->setFontSize(21);
    lshiftButton->setMargins(4, 24, 4, 4);
    lshiftButton->setWidth(120);
    thirdRow->addView(lshiftButton);
    
    for (int i = 19; i < 26; i++)
    {
        ButtonView* button = new ButtonView();
        button->setKey(BUTTONS[i]);
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
    
    ButtonView* ctrlButton = new ButtonView();
    ctrlButton->setKey(VK_LCONTROL);
    ctrlButton->triggerType = true;
    ctrlButton->charLabel->setFontSize(21);
    ctrlButton->setMargins(4, 4, 4, 4);
    ctrlButton->setWidth(120);
    fourthRow->addView(ctrlButton);
    
    ButtonView* altButton = new ButtonView();
    altButton->setKey(VK_LMENU);
    altButton->triggerType = true;
    altButton->charLabel->setFontSize(21);
    altButton->setMargins(4, 4, 4, 4);
    altButton->setWidth(120);
    fourthRow->addView(altButton);
    
    ButtonView* spaceButton = new ButtonView();
    spaceButton->setKey(VK_SPACE);
    spaceButton->charLabel->setFontSize(21);
    spaceButton->setMargins(4, 4, 4, 4);
    spaceButton->setWidth(464);
    fourthRow->addView(spaceButton);
    
    ButtonView* winButton = new ButtonView();
    winButton->setKey(VK_LWIN);
    winButton->triggerType = true;
    winButton->charLabel->setFontSize(21);
    winButton->setMargins(4, 4, 4, 4);
    winButton->setWidth(120);
    fourthRow->addView(winButton);
    
    ButtonView* returnButton = new ButtonView();
    returnButton->setKey(VK_RETURN);
    returnButton->charLabel->setFontSize(21);
    returnButton->setMargins(4, 4, 4, 4);
    returnButton->setWidth(120);
    fourthRow->addView(returnButton);
}
