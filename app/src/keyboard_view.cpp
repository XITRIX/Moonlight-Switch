//
//  keyboard_view.cpp
//  Moonlight
//
//  Created by Даниил Виноградов on 14.06.2021.
//

#include "keyboard_view.hpp"

using namespace brls;

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
    0xBE,
    0xBC,
    0x70,
    0x71,
    0x72,
    0x73,
    0x74,
    0x75,
    0x76,
    0x77,
    0x78,
    0x79,
    0x7A,
    0x7B,
    0x09,
    0x2E,
    0xBA,
    0xBF,
    0xC0,
    0xDB,
    0xDC,
    0xDD,
    0xDE,
};

std::string KeyboardLocalization[_VK_KEY_MAX]
{
    "Remove",
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
    ".",
    ",",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "TAB",
    "Delete",
    ";",
    "/",
    "`",
    "[",
    "\\",
    "]",
    "'",
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
    
    if (keysState[key])
        this->playClickAnimation(false);
}

void ButtonView::registerCallback()
{
    addGestureRecognizer(new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
        if (event != NULL)
        {
            this->playClickAnimation(status.state != GestureState::UNSURE);
            if (status.state == brls::GestureState::END)
                event();
        }
        else if (!triggerType)
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
